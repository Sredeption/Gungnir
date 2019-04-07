#include "Client.h"
#include "ClientException.h"
#include "Logger.h"

namespace Gungnir {

Client::Client(Context *context, const std::string &connectLocator) : context(context), session() {
    session = context->transport->getSession(connectLocator);
}

void Client::get(uint64_t key, Buffer *value, bool *objectExists) {
    GetRpc rpc(this, key, value);
    rpc.wait(objectExists);
}

void Client::put(uint64_t key, const void *buf, uint32_t length) {
    PutRpc rpc(this, key, buf, length);
    rpc.wait();
}

void Client::erase(uint64_t key) {
    EraseRpc rpc(this, key);
    rpc.wait();
}

Iterator Client::scan(uint64_t start, uint64_t end) {
    Iterator iterator;
    ScanRpc rpc(this, start, end, &iterator);
    rpc.wait();
    return iterator;
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

PutRpc::PutRpc(Client *client, uint64_t key, const void *buf, uint32_t length)
    : RpcWrapper(client->context, client->session, sizeof(WireFormat::Put::Response)) {

    WireFormat::Put::Request *reqHdr(allocHeader<WireFormat::Put>());
    reqHdr->key = key;
    reqHdr->length = length;
    request.append(buf, length);

    send();
}

void PutRpc::wait() {
    waitInternal(context->dispatch);
    const WireFormat::Put::Response *respHdr(
        getResponseHeader<WireFormat::Put>());
    if (respHdr->common.status != STATUS_OK)
        ClientException::throwException(HERE, respHdr->common.status);
}

EraseRpc::EraseRpc(Client *client, uint64_t key)
    : RpcWrapper(client->context, client->session, sizeof(WireFormat::Erase::Response)) {
    WireFormat::Erase::Request *reqHdr(allocHeader<WireFormat::Erase>());
    reqHdr->key = key;
    send();
}

void EraseRpc::wait() {
    waitInternal(context->dispatch);
    const WireFormat::Erase::Response *respHdr(
        getResponseHeader<WireFormat::Erase>());
    if (respHdr->common.status != STATUS_OK)
        ClientException::throwException(HERE, respHdr->common.status);
}

ScanRpc::ScanRpc(Client *client, uint64_t start, uint64_t end, Iterator *iterator)
    : RpcWrapper(client->context, client->session, sizeof(WireFormat::Scan::Response), iterator->buffer.get())
      , iterator(iterator) {
    WireFormat::Scan::Request *reqHdr(allocHeader<WireFormat::Scan>());
    reqHdr->start = start;
    reqHdr->end = end;
    send();
}

void ScanRpc::wait() {
    waitInternal(context->dispatch);
    const WireFormat::Scan::Response *respHdr(
        getResponseHeader<WireFormat::Scan>());
    if (respHdr->common.status != STATUS_OK)
        ClientException::throwException(HERE, respHdr->common.status);
    iterator->size = respHdr->size;
    response->truncateFront(sizeof(*respHdr));
}

}
