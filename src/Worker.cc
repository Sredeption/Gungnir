#include "Worker.h"
#include "Logger.h"
#include "Dispatch.h"
#include "WorkerManager.h"
#include "ThreadId.h"
#include "Cycles.h"
#include "Service.h"
#include "ConcurrentSkipList.h"

#include <linux/futex.h>
#include <syscall.h>
#include <unistd.h>

namespace Gungnir {

#define WORKER_EXIT reinterpret_cast<Transport::ServerRpc*>(1)

int Worker::pollMicros = 10000;

void Worker::exit() {
    Dispatch *dispatch = context->dispatch;
    assert(dispatch->isDispatchThread());
    if (exited) {
        // Worker already exited; nothing to do.  This should only happen
        // during tests.
        return;
    }

    // Wait for the worker thread to finish handling any RPCs already
    // queued for it.
    while (busyIndex >= 0) {
        dispatch->poll();
    }

    // Tell the worker thread to exit, and wait for it to actually exit
    // (don't want it referencing the Worker structure anymore, since
    // it could go away).
    handoff(WORKER_EXIT);
    thread->join();
    rpc = nullptr;
    exited = true;
}

void Worker::handoff(Transport::ServerRpc *newRpc) {
    assert(rpc == nullptr);
    rpc = newRpc;

    int prevState = state.exchange(WORKING);
    if (prevState == SLEEPING) {
        if (Worker::futexWake(reinterpret_cast<int *>(&state), 1)
            == -1) {
            Logger::log(HERE, "futexWake failed in Worker::handoff: %s", strerror(errno));
        }
    }
}

bool Worker::performTask() {
    auto *service = dynamic_cast<Service *> (getNextTask());

    if (service == nullptr)
        throw FatalError(HERE, "Worker accepted a unexpected task");

    try {
        service->performTask();
    } catch (RetryException &e) {
        if (this->replySent()) {
            throw FatalError(HERE, "Retry exception thrown after reply sent for %s RPC");
        } else {
            Service::prepareRetryResponse(service->replyPayload, e.minDelayMicros, e.maxDelayMicros, e.message);
        }
    } catch (ClientException &e) {
        if (this->replySent()) {
            throw FatalError(HERE, "exception thrown after reply sent for RPC");
        } else {
            Service::prepareErrorResponse(service->replyPayload, e.status);
        }
    }
    return false;

}

bool Worker::replySent() {
    return (state.load(std::memory_order_acquire) == POSTPROCESSING);
}

void Worker::sendReply() {
    state.store(POSTPROCESSING, std::memory_order_release);
}

void Worker::updateEpoch() {
    localEpoch.store(context->skipList->epoch.load());
}

int Worker::futexWake(int *addr, int count) {
    return static_cast<int>(syscall(SYS_futex, addr, FUTEX_WAKE, count, NULL, NULL, 0));
}

int Worker::futexWait(int *addr, int value) {
    return static_cast<int>(::syscall(SYS_futex, addr, FUTEX_WAIT, value, NULL, NULL, 0));
}

void Worker::workerMain(Worker *worker) {

    worker->threadId = ThreadId::get();

    uint64_t lastIdle = Cycles::rdtsc();

    try {
        uint64_t pollCycles = Cycles::fromNanoseconds(1000 * pollMicros);
        while (true) {
            uint64_t stopPollingTime = lastIdle + pollCycles;

            // Wait for WorkerManager to supply us with some work to do.
            while (worker->state.load(std::memory_order_acquire) != Worker::WORKING) {
                if (lastIdle >= stopPollingTime) {

                    // It's been a long time since we've had any work to do; go
                    // to sleep so we don't waste any more CPU cycles.  Tricky
                    // race condition: the dispatch thread could change the
                    // state to WORKING just before we change it to SLEEPING,
                    // so use an atomic op and only change to SLEEPING if the
                    // current value is POLLING.
                    int expected = Worker::POLLING;
                    if (worker->state.compare_exchange_strong(expected, Worker::SLEEPING)) {
                        if (Worker::futexWait(reinterpret_cast<int *>(&worker->state), Worker::SLEEPING) == -1) {
                            // EWOULDBLOCK means that someone already changed
                            // worker->state, so we didn't block; this is
                            // benign.
                            if (errno != EWOULDBLOCK) {
                                Logger::log(HERE, "futexWait failed in WorkerManager::workerMain: %s", strerror(errno));
                            }
                        }
                    }
                }
                lastIdle = Cycles::rdtsc();
            }
            if (worker->rpc == WORKER_EXIT)
                break;

            worker->updateEpoch();

            Service *service = Service::dispatch(worker, worker->context, worker->rpc);
            worker->schedule(service);
            while (!worker->isIdle()) {
                worker->performTask();
            }

            // Pass the RPC back to the dispatch thread for completion.
            worker->state.store(Worker::POLLING, std::memory_order_release);

            // Update performance statistics.
            uint64_t current = Cycles::rdtsc();
            lastIdle = current;
        }
    } catch (std::exception &e) {
        Logger::log(HERE, "worker: %s", e.what());
        throw; // will likely call std::terminate()
    } catch (...) {
        Logger::log(HERE, "worker");
        throw; // will likely call std::terminate()
    }
}
}
