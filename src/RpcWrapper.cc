#include <utility>

#include "RpcWrapper.h"
#include "Cycles.h"
#include "Dispatch.h"
#include "ClientException.h"
#include "Logger.h"

namespace Gungnir {

RpcWrapper::RpcWrapper(Context *context, Transport::SessionRef session, uint32_t responseHeaderLength, Buffer *response)
    : context(context), request(), response(response), defaultResponse(), state(NOT_STARTED)
      , session(std::move(session)), retryTime(0), responseHeaderLength(responseHeaderLength), responseHeader(nullptr) {
    if (response == nullptr) {
        defaultResponse = std::make_unique<Buffer>();
        this->response = defaultResponse.get();
    }
}

RpcWrapper::~RpcWrapper() {
    cancel();
}

void RpcWrapper::cancel() {
    // This code is potentially tricky, because complete or failed
    // could get invoked concurrently. Fortunately, we know that
    // this can only happen before cancelRequest returns, and
    // neither of these methods does anything besides setting state.
    if ((getState() == IN_PROGRESS) && session) {
        session->cancelRequest(this);
    }
    state.store(CANCELED, std::memory_order_relaxed);
}

void RpcWrapper::completed() {
    state.store(FINISHED, std::memory_order_release);
}

void RpcWrapper::failed() {
    state.store(FAILED, std::memory_order_release);
}

bool RpcWrapper::isReady() {
    RpcState copyOfState = getState();

    if (copyOfState == IN_PROGRESS) {
        return false;
    }

    if (copyOfState == FINISHED) {
        // Retrieve the status value from the response and handle the
        // normal case of success as quickly as possible.  Note: check to
        // make sure the server has returned enough bytes for the header length
        // expected by the wrapper, but we only use the status word here.
        responseHeader = static_cast<const WireFormat::ResponseCommon *>(
            response->getRange(0, responseHeaderLength));
        if ((responseHeader != nullptr) &&
            (responseHeader->status == STATUS_OK)) {
            return true;
        }

        // We only get to here if something unusual happened. Work through
        // all of the special cases one at a time.
        if (responseHeader == nullptr) {
            // Not enough bytes in the response for a full header; check to see
            // if there is at least a status value. Note: we're asking for
            // fewer bytes here than in the getRange above (just enough for
            // the status info instead of the entire response header that
            // higher level wrapper will want).
            responseHeader = static_cast<const WireFormat::ResponseCommon *>(
                response->getRange(0, sizeof(WireFormat::ResponseCommon)));
            if ((responseHeader == nullptr)
                || (responseHeader->status == STATUS_OK)) {
                Logger::log(HERE, "Response from %s for RPC is too short "
                                  "(needed at least %d bytes, got %d)",
                            session->serviceLocator.c_str(),
                            responseHeaderLength,
                            static_cast<int>(response->size()));
                throw MessageErrorException(HERE);
            }
            // Extend the response buffer to be at least responseHeaderLength
            // bytes long.
            if (response->size() < responseHeaderLength) {
                response->alloc(responseHeaderLength - response->size());
            }
        }
        if (responseHeader->status == STATUS_RETRY) {
            // The server wants us to try again; typically this means that
            // it is overloaded, or there is some reason why the operation
            // can't complete right away and it's better for us to wait
            // here, rather than occupy resources on the server by waiting
            // there.
            WireFormat::RetryResponse defaultResponse =
                {{STATUS_RETRY}, 100, 200, 0};
            const WireFormat::RetryResponse *retryResponse =
                response->getStart<WireFormat::RetryResponse>();
            if (retryResponse == nullptr) {
                retryResponse = &defaultResponse;
            }
            retry(retryResponse->minDelayMicros,
                  retryResponse->maxDelayMicros);
            return false;
        }

        // The server returned an error status that we don't know how to
        // process; see if a subclass knows what to do with it.
        return checkStatus();
    }

    if (copyOfState == RETRY) {
        if (Cycles::rdtsc() >= retryTime) {
            send();
        }
        return false;
    }

    if (copyOfState == CANCELED) {
        return true;
    }

    if (copyOfState == FAILED) {
        // There was a transport-level failure, which the transport should
        // already have logged. Invoke a subclass to decide whether or not
        // to retry.
        return handleTransportError();
    }

    Logger::log(HERE, "RpcWrapper::isReady found unknown state %d ");
    throw InternalError(HERE, STATUS_INTERNAL_ERROR);
}

bool RpcWrapper::checkStatus() {
    return true;
}

bool RpcWrapper::handleTransportError() {
    return true;
}

void RpcWrapper::retry(uint32_t minDelayMicros, uint32_t maxDelayMicros) {
    uint64_t delay = minDelayMicros;
    if (minDelayMicros != maxDelayMicros) {
        delay += randomNumberGenerator(maxDelayMicros + 1 - minDelayMicros);
    }
    retryTime = Cycles::rdtsc() + Cycles::fromNanoseconds(1000 * delay);
    state = RETRY;
}

void RpcWrapper::send() {
    state.store(IN_PROGRESS, std::memory_order_relaxed);
    if (session)
        session->sendRequest(&request, response, this);
    else
        throw FatalError(HERE, "No available session");
}

void RpcWrapper::simpleWait(Context *context) {
    waitInternal(context->dispatch);
    if (responseHeader->status != STATUS_OK)
        ClientException::throwException(HERE, responseHeader->status);
}

const char *RpcWrapper::stateString() {
    switch (state) {
        case RpcWrapper::RpcState::NOT_STARTED:
            return "NOT_STARTED";
        case RpcWrapper::RpcState::IN_PROGRESS:
            return "IN_PROGRESS";
        case RpcWrapper::RpcState::FINISHED:
            return "FINISHED";
        case RpcWrapper::RpcState::FAILED:
            return "FAILED";
        case RpcWrapper::RpcState::CANCELED:
            return "CANCELED";
        case RpcWrapper::RpcState::RETRY:
            return "RETRY";
    }
    static char buffer[100];
    snprintf(buffer, sizeof(buffer), "unknown (%d)", int(state));
    return buffer;
}

bool RpcWrapper::waitInternal(Dispatch *dispatch) {
    // When invoked in RAMCloud servers there is a separate dispatch thread,
    // so we just busy-wait here. When invoked on RAMCloud clients we're in
    // the dispatch thread so we have to invoke the dispatcher while waiting.
    bool isDispatchThread = dispatch->isDispatchThread();

    while (!isReady()) {
        if (isDispatchThread)
            dispatch->poll();
    }
    if (getState() == CANCELED)
        throw RpcCanceledException(HERE);
    return true;
}


}
