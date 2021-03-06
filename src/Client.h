#ifndef GUNGNIR_CLIENT_H
#define GUNGNIR_CLIENT_H


#include "Context.h"
#include "Buffer.h"
#include "Iterator.h"
#include "Transport.h"
#include "RpcWrapper.h"

namespace Gungnir {

class Client {

public:
    explicit Client(Context *context, const std::string &connectLocator);

    void get(uint64_t key, Buffer *value, bool *objectExists = nullptr);

    void put(uint64_t key, const void *buf, uint32_t length);

    void erase(uint64_t key);

    Iterator scan(uint64_t startKey, uint64_t lastKey);

    Context *context;

    Transport::SessionRef session;

};

class GetRpc : public RpcWrapper {
public:
    GetRpc(Client *client, uint64_t key, Buffer *value);

    void wait(bool *objectExists);
};

class PutRpc : public RpcWrapper {
public:
    PutRpc(Client *client, uint64_t key, const void *buf, uint32_t length);

    void wait();
};

class EraseRpc : public RpcWrapper {
public:
    EraseRpc(Client *client, uint64_t key);

    void wait();
};

class ScanRpc : public RpcWrapper {
public:
    ScanRpc(Client *client, uint64_t start, uint64_t end, Iterator *iterator);

    void wait();

private:
    Iterator *iterator;
};

}

#endif //GUNGNIR_CLIENT_H
