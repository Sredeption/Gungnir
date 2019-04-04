#include "Client.h"
#include "ClientException.h"

namespace Gungnir {

Client::Client(Context *context, const std::string &connectLocator) : context(context), session() {
    session = context->transport->getSession(connectLocator);
}

void Client::get(uint64_t key, Buffer *value, bool *objectExists) {
    GetRpc rpc(this, key, value);
    rpc.wait(objectExists);
}

void Client::put(uint64_t key, const void *buf, uint32_t length) {

}

void Client::erase(uint64_t key) {

}

Iterator Client::scan(uint64_t startKey, uint64_t lastKey) {
    return Iterator();
}

GetRpc::GetRpc(Client *client, uint64_t key, Buffer *value)
    : RpcWrapper(client->context, client->session, sizeof(WireFormat::Get::Response), value) {
    value->reset();
    WireFormat::Get::Request *reqHdr(allocHeader<WireFormat::Get>());
    reqHdr->key = key;
    send();

}

void GetRpc::wait(bool *objectExists) {

    if (objectExists != nullptr)
        *objectExists = true;

    waitInternal(context->dispatch);

    const WireFormat::Get::Response *respHdr(
        getResponseHeader<WireFormat::Get>());

    if (respHdr->common.status != STATUS_OK) {
        if (objectExists != nullptr &&
            respHdr->common.status == STATUS_OBJECT_DOESNT_EXIST) {
            *objectExists = false;
        } else {
            ClientException::throwException(HERE, respHdr->common.status);
        }
    }

    response->truncateFront(sizeof(*respHdr));
    assert(respHdr->length == response->size());
}
}