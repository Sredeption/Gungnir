#include "Service.h"
#include "ClientException.h"
#include "Cycles.h"
#include "Logger.h"
#include "ConcurrentSkipList.h"

namespace Gungnir {

void Service::prepareErrorResponse(Buffer *replyPayload, Status status) {
    auto *responseCommon = const_cast<WireFormat::ResponseCommon *>(
        replyPayload->getStart<WireFormat::ResponseCommon>());
    if (responseCommon == nullptr) {
        responseCommon =
            replyPayload->emplaceAppend<WireFormat::ResponseCommon>();
    }
    responseCommon->status = status;
}

void Service::prepareRetryResponse(Buffer *replyPayload, uint32_t minDelayMicros, uint32_t maxDelayMicros,
                                   const char *message) {
    replyPayload->reset();
    auto *response = replyPayload->emplaceAppend<WireFormat::RetryResponse>();
    response->common.status = STATUS_RETRY;
    response->minDelayMicros = minDelayMicros;
    response->maxDelayMicros = maxDelayMicros;
    if (message != nullptr) {
        response->messageLength = static_cast<uint32_t>(strlen(message) + 1);
        replyPayload->append(message, response->messageLength);
    } else {
        response->messageLength = 0;
    }
}

Service *Service::dispatch(Worker *worker, Context *context, Transport::ServerRpc *rpc) {
    const WireFormat::RequestCommon *header;
    header = rpc->requestPayload.getStart<WireFormat::RequestCommon>();

    auto opcode = WireFormat::Opcode(header->opcode);
    switch (opcode) {
        case WireFormat::GET:
            return new GetService(worker, context, rpc);
        case WireFormat::PUT:
            return new PutService(worker, context, rpc);
        case WireFormat::ERASE:
            return new EraseService(worker, context, rpc);
        case WireFormat::SCAN:
            return new ScanService(worker, context, rpc);
        default:
            return nullptr;
    }
}

Service::Service(Worker *worker, Context *context, Transport::ServerRpc *rpc)
    : worker(worker), context(context), requestPayload(&rpc->requestPayload), replyPayload(&rpc->replyPayload)
      , skipList(context->skipList) {

}

GetService::GetService(Worker *worker, Context *context, Transport::ServerRpc *rpc)
    : Service(worker, context, rpc) {
}

void GetService::performTask() {
    auto *reqHdr = requestPayload->getStart<WireFormat::Get::Request>();
    Key key(reqHdr->key);

    auto *respHdr = replyPayload->emplaceAppend<WireFormat::Get::Response>();
    ConcurrentSkipList::Node *node = skipList->find(key);
    if (node != nullptr && !node->markedForRemoval()) {
        respHdr->common.status = STATUS_OK;
        Object *object = node->getObject();
        if (object != nullptr) {
            replyPayload->append(&object->value);
            respHdr->length = object->value.size();
            respHdr->common.status = STATUS_OK;
            return;
        }
    }
    respHdr->common.status = STATUS_OBJECT_DOESNT_EXIST;
}

PutService::PutService(Worker *worker, Context *context, Transport::ServerRpc *rpc)
    : Service(worker, context, rpc), state(FIND), node(nullptr), guard(), object(nullptr) {
    auto *respHdr = replyPayload->emplaceAppend<WireFormat::Put::Response>();
    respHdr->common.status = STATUS_OK;
}

void PutService::performTask() {
    auto *reqHdr = requestPayload->getStart<WireFormat::Put::Request>();

    Key key(reqHdr->key);
    if (state == FIND) {
        node = skipList->addOrGetNode(key);
        if (node == nullptr) {
            schedule();
            return;
        } else
            state = LOCK;
    }
    if (state == LOCK) {
        for (int i = 0; i < 10; i++) {
            guard = node->tryAcquireGuard();
            if (guard.owns_lock()) {
                break;
            }
        }
        if (guard.owns_lock()) {
            if (node->markedForRemoval()) {
                guard.unlock();
                state = FIND;
                schedule();
                return;
            }

            requestPayload->truncateFront(sizeof(*reqHdr));
            object = new Object(key, requestPayload);
            state = WRITE;
        } else {
            schedule();
            return;
        }
    }
    if (state == WRITE) {
        Object *old = node->setObject(object);
        skipList->destroy(old);
        guard.unlock();
        state = DONE;
    }
}

EraseService::EraseService(Worker *worker, Context *context, Transport::ServerRpc *rpc)
    : Service(worker, context, rpc), state(FIND), nodeToDelete(nullptr), nodeGuard(), isMarked(false), nodeHeight(0)
      , predecessors(), successors(), maxLayer(0), layer() {
}

void EraseService::performTask() {
    auto *reqHdr = requestPayload->getStart<WireFormat::Erase::Request>();
    Key key(reqHdr->key);

    if (state == FIND) {
        maxLayer = 0;
        layer = skipList->findInsertionPointGetMaxLayer(key, predecessors, successors, &maxLayer);
        if (!isMarked && (layer < 0 || !ConcurrentSkipList::okToDelete(successors[layer], layer))) {
            state = DONE;
            return;
        }
        state = MARK;
    }

    if (state == MARK) {
        if (!isMarked) {
            nodeToDelete = successors[layer];
            nodeHeight = nodeToDelete->getHeight();
            nodeGuard = nodeToDelete->tryAcquireGuard();
            for (int i = 0; i < 10; i++) {
                if (nodeGuard.owns_lock()) {
                    break;
                }
            }
            if (!nodeGuard.owns_lock()) {
                schedule();
                return;
            }
            if (nodeToDelete->markedForRemoval()) {
                state = DONE;
                return;
            }
            nodeToDelete->setMarkedForRemoval();
            isMarked = true;
            nodeGuard.unlock();
        }
        state = LOCK;
    }

    if (state == LOCK) {
        ConcurrentSkipList::LayerLocker guards;
        bool locked = false;
        for (int i = 0; i < 10; i++) {
            locked = ConcurrentSkipList::tryLockNodesForChange(nodeHeight, guards, predecessors, successors, false);
            if (locked)
                break;
        }
        if (locked) {
            for (int k = nodeHeight - 1; k >= 0; --k) {
                predecessors[k]->setSkip(k, nodeToDelete->skip(k));
            }
            state = DELETE;
        } else {
            schedule();
            return;
        }
    }

    if (state == DELETE) {
        skipList->destroy(nodeToDelete);
        state = DONE;
    }

}

ScanService::ScanService(Worker *worker, Context *context, Transport::ServerRpc *rpc)
    : Service(worker, context, rpc) {


}

void ScanService::performTask() {

}
}
