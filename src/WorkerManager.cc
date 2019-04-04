#include "WorkerManager.h"
#include "Logger.h"
#include "ThreadId.h"
#include "Cycles.h"
#include "TaskQueue.h"
#include "Service.h"

#include <linux/futex.h>
#include <syscall.h>
#include <unistd.h>


namespace Gungnir {

int WorkerManager::pollMicros = 10000;

#define WORKER_EXIT reinterpret_cast<Transport::ServerRpc*>(1)

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
        if (WorkerManager::futexWake(reinterpret_cast<int *>(&state), 1)
            == -1) {
            Logger::log(HERE, "futexWake failed in Worker::handoff: %s", strerror(errno));
        }
    }
}

bool Worker::replySent() {
    return (state.load(std::memory_order_acquire) == POSTPROCESSING);
}

void Worker::sendReply() {
    state.store(POSTPROCESSING, std::memory_order_release);
}

WorkerManager::WorkerManager(Context *context, uint32_t maxCores)
    : Dispatch::Poller(context->dispatch, "WorkerManager")
      , context(context), waitingRpcs(), busyThreads(), idleThreads(), maxCores(maxCores), rpcsWaiting(0) {
    for (uint32_t i = 0; i < maxCores; i++) {
        auto *worker = new Worker(context);
        worker->thread = std::make_unique<std::thread>(workerMain, worker);
        idleThreads.push_back(worker);
    }
}

WorkerManager::~WorkerManager() {
    Dispatch *dispatch = context->dispatch;
    assert(dispatch->isDispatchThread());
    while (!busyThreads.empty()) {
        dispatch->poll();
    }
    for (Worker *worker: idleThreads) {
        worker->exit();
        delete worker;
    }
}

void WorkerManager::handleRpc(Transport::ServerRpc *rpc) {
    // Find the service for this RPC.
    const WireFormat::RequestCommon *header;
    header = rpc->requestPayload.getStart<WireFormat::RequestCommon>();
    if ((header == nullptr) || (header->opcode >= WireFormat::ILLEGAL_RPC_TYPE)) {
        if (header == nullptr) {
            Logger::log(HERE, "Incoming RPC contains no header (message length %d)",
                        rpc->requestPayload.size());
            Service::prepareErrorResponse(&rpc->replyPayload,
                                          STATUS_MESSAGE_ERROR);
        } else {
            Logger::log(HERE, "Incoming RPC contained unknown opcode %d",
                        header->opcode);
            Service::prepareErrorResponse(&rpc->replyPayload,
                                          STATUS_UNIMPLEMENTED_REQUEST);
        }
        rpc->sendReply();
        return;
    }

    if (idleThreads.empty()) {
        waitingRpcs.push(rpc);
        rpcsWaiting++;
    }

    assert(!idleThreads.empty());
    Worker *worker = idleThreads.back();
    idleThreads.pop_back();
    worker->opcode = WireFormat::Opcode(header->opcode);
    worker->handoff(rpc);
    worker->busyIndex = static_cast<int>(busyThreads.size());
    busyThreads.push_back(worker);
}

bool WorkerManager::idle() {
    return busyThreads.empty();
}

int WorkerManager::poll() {
    int foundWork = 0;

    // Each iteration of the following loop checks the status of one active
    // worker. The order of iteration is crucial, since it allows us to
    // remove a worker from busyThreads in the middle of the loop without
    // interfering with the remaining iterations.
    for (int i = static_cast<int>(busyThreads.size()) - 1; i >= 0; i--) {
        Worker *worker = busyThreads[i];
        assert(worker->busyIndex == i);
        int state = worker->state.load(std::memory_order_acquire);
        if (state == Worker::WORKING) {
            continue;
        }
        foundWork = 1;
        std::atomic_thread_fence(std::memory_order_acquire);

        // The worker is either post-processing or idle; in either case,
        // there may be an RPC that we have to respond to. Save the RPC
        // information for now.
        Transport::ServerRpc *rpc = worker->rpc;
        worker->rpc = nullptr;

        // Highest priority: if there are pending requests that are waiting
        // for workers, hand off a new request to this worker ASAP.
        bool startedNewRpc = false;
        if (state != Worker::POSTPROCESSING) {
            if (rpcsWaiting) {
                rpcsWaiting--;
                worker->handoff(waitingRpcs.front());
                waitingRpcs.pop();
                startedNewRpc = true;
            }
        }

        // Now send the response, if any.
        if (rpc != nullptr) {
            rpc->sendReply();
        }

        // If the worker is idle, remove it from busyThreads (fill its
        // slot with the worker in the last slot).
        if (!startedNewRpc && (state != Worker::POSTPROCESSING)) {
            if (worker != busyThreads.back()) {
                busyThreads[worker->busyIndex] = busyThreads.back();
                busyThreads[worker->busyIndex]->busyIndex = worker->busyIndex;
            }
            busyThreads.pop_back();
            worker->busyIndex = -1;
            idleThreads.push_back(worker);
        }
    }
    return foundWork;
}

Transport::ServerRpc *WorkerManager::waitForRpc(double timeoutSeconds) {
    uint64_t start = Cycles::rdtsc();
    while (true) {
        if (Cycles::toSeconds(Cycles::rdtsc() - start) > timeoutSeconds) {
            return nullptr;
        }
        context->dispatch->poll();
    }
}

int WorkerManager::futexWake(int *addr, int count) {
    return static_cast<int>(syscall(SYS_futex, addr, FUTEX_WAKE, count, NULL, NULL, 0));
}

int WorkerManager::futexWait(int *addr, int value) {
    return static_cast<int>(::syscall(SYS_futex, addr, FUTEX_WAIT, value, NULL, NULL, 0));
}

void WorkerManager::workerMain(Worker *worker) {

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
                        if (WorkerManager::futexWait(reinterpret_cast<int *>(&worker->state), Worker::SLEEPING) == -1) {
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

            Service::Rpc rpc(worker, &worker->rpc->requestPayload,
                             &worker->rpc->replyPayload);
            Service::handleRpc(worker->context, &rpc);

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
