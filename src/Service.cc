#include "Service.h"
#include "ClientException.h"
#include "Cycles.h"
#include "Logger.h"

namespace Gungnir {

void Service::prepareErrorResponse(Buffer *replyPayload, Status status) {
    auto *responseCommon = const_cast<WireFormat::ResponseCommon *>(
        replyPayload->getStart<WireFormat::ResponseCommon>());
    if (responseCommon == nullptr) {
        // Response is currently empty; add a header to it.
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

void Service::handleRpc(Context *context, Service::Rpc *rpc) {
    const WireFormat::RequestCommon *header;
    header = rpc->requestPayload->getStart<WireFormat::RequestCommon>();
    if (header == nullptr) {
        prepareErrorResponse(rpc->replyPayload, STATUS_MESSAGE_ERROR);
        return;
    }

    auto opcode = WireFormat::Opcode(header->opcode);
    try {
        if (opcode == WireFormat::Get::opcode) {
            auto *respHdr = rpc->replyPayload->emplaceAppend<WireFormat::Get::Response>();
            respHdr->length = 0;
        }
    } catch (RetryException &e) {
        if (rpc->worker->replySent()) {
            throw FatalError(HERE, "Retry exception thrown after reply sent for %s RPC");
        } else {
            prepareRetryResponse(rpc->replyPayload, e.minDelayMicros,
                                 e.maxDelayMicros, e.message);
        }
    } catch (ClientException &e) {
        if (rpc->worker->replySent()) {
            throw FatalError(HERE, "exception thrown after reply sent for RPC");
        } else {
            prepareErrorResponse(rpc->replyPayload, e.status);
        }
    }
}

void Service::read(const WireFormat::Get::Request *reqHdr,
                   WireFormat::Get::Response *respHdr, Service::Rpc *rpc) {
    respHdr->length = 0;
}

void Service::Rpc::sendReply() {
    worker->sendReply();
}
}
