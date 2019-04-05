#ifndef GUNGNIR_WORKER_H
#define GUNGNIR_WORKER_H

#include "WireFormat.h"
#include "Transport.h"
#include "TaskQueue.h"
#include "Context.h"

#include <thread>

namespace Gungnir {

class Worker : public TaskQueue {

public:
    bool replySent();

    void sendReply();

private:

    std::unique_ptr<std::thread> thread;

public:
    int threadId;
    WireFormat::Opcode opcode;
    Transport::ServerRpc *rpc;

    static int pollMicros;

    static int futexWake(int *addr, int count);

    static int futexWait(int *addr, int value);

    static void workerMain(Worker *worker);

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
        : TaskQueue(context), thread(), threadId(0), opcode(WireFormat::Opcode::ILLEGAL_RPC_TYPE)
          , rpc(nullptr), busyIndex(-1), state(POLLING), exited(false) {}

    void exit();

    void handoff(Transport::ServerRpc *newRpc);

    bool performTask() override;


    friend class WorkerManager;
};

}

#endif //GUNGNIR_WORKER_H
