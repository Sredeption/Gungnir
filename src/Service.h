#ifndef GUNGNIR_SERVICE_H
#define GUNGNIR_SERVICE_H

#include "WorkerManager.h"
#include "ClientException.h"
#include "TaskQueue.h"

namespace Gungnir {

class Service : public Task {

public:
    Service(Worker *worker, Context *context, Transport::ServerRpc *rpc);

    static void prepareErrorResponse(Buffer *replyPayload, Status status);

    static void prepareRetryResponse(
        Buffer *replyPayload, uint32_t minDelayMicros, uint32_t maxDelayMicros, const char *message);

    static Service *dispatch(Worker *worker, Context *context, Transport::ServerRpc *rpc);

public:
    Worker *worker;
    Context *context;
    Buffer *requestPayload;
    Buffer *replyPayload;

};

class GetService : public Service {
public:
    GetService(Worker *worker, Context *context, Transport::ServerRpc *rpc);

    void performTask() override;
};

class PutService : public Service {
public:
    PutService(Worker *worker, Context *context, Transport::ServerRpc *rpc);

    void performTask() override;
};

class EraseService : public Service {
public:
    EraseService(Worker *worker, Context *context, Transport::ServerRpc *rpc);

    void performTask() override;
};

class ScanService : public Service {
public:
    ScanService(Worker *worker, Context *context, Transport::ServerRpc *rpc);

    void performTask() override;
};

}


#endif //GUNGNIR_SERVICE_H
