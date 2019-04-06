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
    auto *reqHdr = requestPayload->getStart<WireFormat::Put::Request>();
    Key key(reqHdr->key);

    auto *respHdr = replyPayload->emplaceAppend<WireFormat::Get::Response>();
    ConcurrentSkipList::Node *node = skipList->find(key);
    if (node != nullptr && !node->markedForRemoval()) {
        respHdr->common.status = STATUS_OK;
        respHdr->length = 0;
    } else {
        respHdr->common.status = STATUS_OBJECT_DOESNT_EXIST;
    }
}

PutService::PutService(Worker *worker, Context *context, Transport::ServerRpc *rpc)
    : Service(worker, context, rpc), state(FIND), node(nullptr), guard() {
    auto *respHdr = replyPayload->emplaceAppend<WireFormat::Put::Response>();
    respHdr->common.status = STATUS_OK;
}

void PutService::performTask() {
    auto *reqHdr = requestPayload->getStart<WireFormat::Put::Request>();
    requestPayload->getRange(sizeof(*reqHdr), reqHdr->length);
    Key key(reqHdr->key);
    if (state == FIND) {
        node = skipList->addOrGetNode(key);
        if (node == nullptr)
            return;
        else
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
                return;
            }
            state = INSERT;
        } else {
            return;
        }
    }
    if (state == INSERT) {
        state = DONE;
    }
}

EraseService::EraseService(Worker *worker, Context *context, Transport::ServerRpc *rpc)
    : Service(worker, context, rpc) {

    ConcurrentSkipList *skipList = context->skipList;
    auto *reqHdr = requestPayload->getStart<WireFormat::Erase::Request>();
    Key key(reqHdr->key);
    skipList->remove(key);
}

void EraseService::performTask() {

}

ScanService::ScanService(Worker *worker, Context *context, Transport::ServerRpc *rpc)
    : Service(worker, context, rpc) {


}

void ScanService::performTask() {

}
}
