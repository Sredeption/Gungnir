#ifndef GUNGNIR_WORKERMANAGER_H
#define GUNGNIR_WORKERMANAGER_H

#include <queue>

#include "WireFormat.h"
#include "Transport.h"
#include "Dispatch.h"
#include "Context.h"

namespace Gungnir {

class Worker {

public:
    bool replySent();

    void sendReply();

private:

    Context *context;
    std::unique_ptr<std::thread> thread;

public:
    int threadId;
    WireFormat::Opcode opcode;
    Transport::ServerRpc *rpc;
private:
    int busyIndex;
    std::atomic<int> state;
    enum {
        POLLING,
        WORKING,
        POSTPROCESSING,
        SLEEPING
    };
    bool exited;

    explicit Worker(Context *context)
        : context(context), thread(), threadId(0), opcode(WireFormat::Opcode::ILLEGAL_RPC_TYPE), rpc(nullptr)
          , busyIndex(-1), state(POLLING), exited(false) {}

    void exit();

    void handoff(Transport::ServerRpc *newRpc);

    friend class WorkerManager;
};

class WorkerManager : Dispatch::Poller {
public:

    explicit WorkerManager(Context *context, uint32_t maxCores);

    ~WorkerManager() override;

    void handleRpc(Transport::ServerRpc *rpc);

    bool idle();

    int poll() override;

    Transport::ServerRpc *waitForRpc(double timeoutSeconds);

    static int futexWake(int *addr, int count);

    static int futexWait(int *addr, int value);

private:

    /// How many microseconds worker threads should remain in their polling
    /// loop waiting for work. If no new arrives during this period the
    /// worker thread will put itself to sleep, which releases its core but
    /// will result in additional delay for the next RPC while it wakes up.
    /// The value of this variable is typically not modified except during
    /// testing.
    static int pollMicros;

    Context *context;
    std::queue<Transport::ServerRpc *> waitingRpcs;

    std::vector<Worker *> busyThreads;
    std::vector<Worker *> idleThreads;
    uint32_t maxCores;
    int rpcsWaiting;

    static void workerMain(Worker *worker);

};


}


#endif //GUNGNIR_WORKERMANAGER_H
