#include "WorkerManager.h"
#include "Logger.h"
#include "ThreadId.h"
#include "Cycles.h"
#include "TaskQueue.h"
#include "Service.h"


namespace Gungnir {


WorkerManager::WorkerManager(Context *context, uint32_t maxCores)
    : Dispatch::Poller(context->dispatch, "WorkerManager")
      , context(context), waitingRpcs(), busyThreads(), idleThreads(), maxCores(maxCores), rpcsWaiting(0) {
    for (uint32_t i = 0; i < maxCores; i++) {
        auto *worker = new Worker(context);
        worker->thread = std::make_unique<std::thread>(Worker::workerMain, worker);
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
            Service::prepareErrorResponse(&rpc->replyPayload, STATUS_MESSAGE_ERROR);
        } else {
            Logger::log(HERE, "Incoming RPC contained unknown opcode %d",
                        header->opcode);
            Service::prepareErrorResponse(&rpc->replyPayload, STATUS_UNIMPLEMENTED_REQUEST);
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


}
