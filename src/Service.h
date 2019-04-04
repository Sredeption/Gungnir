#ifndef GUNGNIR_SERVICE_H
#define GUNGNIR_SERVICE_H

#include "WorkerManager.h"

namespace Gungnir {

class Service {

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

        friend class WorkerManager;

        friend class Service;

    };

    static void prepareErrorResponse(Buffer *replyPayload, Status status);

    static void prepareRetryResponse(Buffer *replyPayload,
                                     uint32_t minDelayMicros,
                                     uint32_t maxDelayMicros,
                                     const char *message);

    static void handleRpc(Context *context, Rpc *rpc);
};


}


#endif //GUNGNIR_SERVICE_H
