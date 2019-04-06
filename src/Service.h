#ifndef GUNGNIR_SERVICE_H
#define GUNGNIR_SERVICE_H

#include "WorkerManager.h"
#include "ClientException.h"
#include "ConcurrentSkipList.h"
#include "TaskQueue.h"
#include "Key.h"

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
    ConcurrentSkipList *skipList;
};

class GetService : public Service {
public:
    GetService(Worker *worker, Context *context, Transport::ServerRpc *rpc);

    void performTask() override;
};

class PutService : public Service {
public:
    enum State {
        FIND,
        LOCK,
        WRITE,
        DONE
    };

    PutService(Worker *worker, Context *context, Transport::ServerRpc *rpc);

    void performTask() override;

private:
    State state;
    ConcurrentSkipList::Node *node;
    ConcurrentSkipList::ScopedLocker guard;
    Object *object;
};

class EraseService : public Service {
public:
    enum State {
        FIND,
        MARK,
        LOCK,
        DELETE,
        DONE
    };

    EraseService(Worker *worker, Context *context, Transport::ServerRpc *rpc);

    void performTask() override;

private:
    State state;
    ConcurrentSkipList::Node *nodeToDelete;
    ConcurrentSkipList::ScopedLocker nodeGuard;
    bool isMarked;
    int nodeHeight;
    ConcurrentSkipList::Node *predecessors[MAX_HEIGHT], *successors[MAX_HEIGHT];
    int maxLayer;
    int layer;
};

class ScanService : public Service {
public:
    ScanService(Worker *worker, Context *context, Transport::ServerRpc *rpc);

    void performTask() override;
};

}


#endif //GUNGNIR_SERVICE_H
