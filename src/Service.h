#ifndef GUNGNIR_SERVICE_H
#define GUNGNIR_SERVICE_H

#include "WorkerManager.h"
#include "ClientException.h"
#include "TaskQueue.h"

namespace Gungnir {

class Service : public Task {

public:
    class Rpc {
    public:
        /**
         * Constructor for Rpc.
         */
        Rpc(Worker *worker, Buffer *requestPayload, Buffer *replyPayload)
            : requestPayload(requestPayload)
              , replyPayload(replyPayload)
              , worker(worker) {}

        void sendReply();

        /// The incoming request, which describes the desired operation.
        Buffer *requestPayload;

        /// The response, which will eventually be returned to the client.
        Buffer *replyPayload;

        /// Information about the worker thread that is executing
        /// this request.
        Worker *worker;

    };

    Service(Worker *worker, Context *context, Transport::ServerRpc *rpc);

    static void prepareErrorResponse(Buffer *replyPayload, Status status);

    static void prepareRetryResponse(
        Buffer *replyPayload, uint32_t minDelayMicros, uint32_t maxDelayMicros, const char *message);


    static Service *dispatch(Worker *worker,Context *context, Transport::ServerRpc *rpc);

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
