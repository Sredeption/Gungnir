#ifndef GUNGNIR_WORKERMANAGER_H
#define GUNGNIR_WORKERMANAGER_H

#include <queue>

#include "WireFormat.h"
#include "Transport.h"
#include "Dispatch.h"
#include "Context.h"
#include "TaskQueue.h"
#include "Worker.h"

namespace Gungnir {


class WorkerManager : Dispatch::Poller {
public:

    explicit WorkerManager(Context *context, uint32_t maxCores);

    ~WorkerManager() override;

    void handleRpc(Transport::ServerRpc *rpc);

    bool idle();

    int poll() override;

    Transport::ServerRpc *waitForRpc(double timeoutSeconds);

private:

    Context *context;
    std::queue<Transport::ServerRpc *> waitingRpcs;

    std::vector<Worker *> busyThreads;
    std::vector<Worker *> idleThreads;
    uint32_t maxCores;
    int rpcsWaiting;


};


}


#endif //GUNGNIR_WORKERMANAGER_H
