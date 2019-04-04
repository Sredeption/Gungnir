#ifndef GUNGNIR_RPCWRAPPER_H
#define GUNGNIR_RPCWRAPPER_H

#include "WireFormat.h"
#include "Transport.h"
#include "Context.h"

namespace Gungnir {

class RpcWrapper : public Transport::RpcNotifier {
public:
    explicit RpcWrapper(uint32_t responseHeaderLength,
                        Buffer *response = nullptr);

    ~RpcWrapper() override;

    void cancel();

    void completed() override;

    void failed() override;

    virtual bool isReady();

protected:
    /// Possible states for an RPC.
    enum RpcState {
        NOT_STARTED,                // Initial state before the request has been sent for the first time.
        IN_PROGRESS,                // A request has been sent but no response has been received.
        FINISHED,                   // The RPC has completed and the server's response is available in response.
        FAILED,                     // The RPC has failed due to a transport error; most likely it will be retried.
        CANCELED,                   // The RPC has been canceled, so it will never complete.
        RETRY                       // The RPC needs to be retried at the time given by retryTime.
    };

    Buffer request;
    Buffer *response;

    /// Storage to use for response if constructor was not given an explicit
    /// response buffer.
    std::unique_ptr<Buffer> defaultResponse;

    std::atomic<RpcState> state;

    /// Session on which RPC has been sent, or NULL if none.
    Transport::SessionRef session;

    /// Retry the RPC when Cycles::rdtsc reaches this value.
    uint64_t retryTime;

    /// Expected size of the response header, in bytes.
    const uint32_t responseHeaderLength;

    const WireFormat::ResponseCommon *responseHeader;

    template<typename RpcType>
    typename RpcType::Request *
    allocHeader() {
        assert(request.size() == 0);
        typename RpcType::Request *reqHdr =
            request.emplaceAppend<typename RpcType::Request>();
        // Don't allow this method to be used for RPCs that use
        // RequestCommonWithId as the header; use the other form
        // with targetId instead.
        static_assert(sizeof(reqHdr->common) ==
                      sizeof(WireFormat::RequestCommon),
                      "must pass targetId to allocHeader");
        memset(reqHdr, 0, sizeof(*reqHdr));
        reqHdr->common.opcode = RpcType::opcode;
        return reqHdr;
    }

    virtual bool checkStatus();

    template<typename RpcType>
    const typename RpcType::Request *
    getRequestHeader() {
        assert(request.size() >= sizeof(typename RpcType::Request));
        return request.getOffset<const typename RpcType::Request>(0);
    }

    template<typename RpcType>
    const typename RpcType::Response *
    getResponseHeader() {
        assert(responseHeader != nullptr);
        assert(responseHeaderLength >= sizeof(typename RpcType::Response));
        return reinterpret_cast<const typename RpcType::Response *>(
            responseHeader);
    }


    RpcState getState() {
        return state.load(std::memory_order_acquire);
    }

    virtual bool handleTransportError();

    void retry(uint32_t minDelayMicros, uint32_t maxDelayMicros);

    virtual void send();

    void simpleWait(Context *context);

    const char *stateString();

    bool waitInternal(Dispatch *dispatch);
};

}


#endif //GUNGNIR_RPCWRAPPER_H
