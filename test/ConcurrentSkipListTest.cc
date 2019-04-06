#include <gtest/gtest.h>
#include <set>
#include "Logger.h"
#include "Worker.h"
#include "ConcurrentSkipList.h"
#include "LogCleaner.h"
#include "Service.h"

namespace Gungnir {

class TestRpc : public Transport::ServerRpc {

    void sendReply() override {}

    std::string getClientServiceLocator() override {}

};

struct ConcurrentSkipListTest : public ::testing::Test {
    Context *context;
    Worker *worker;
    std::string DOESNT_EXISTS;

    ConcurrentSkipListTest() : context(), worker(), DOESNT_EXISTS("DOESNT_EXISTS") {
        context = new Context();
        context->skipList = new ConcurrentSkipList(context);
        context->logCleaner = new LogCleaner(context);
        worker = new Worker(context);
    }


    TestRpc *getRpc(uint64_t key) {
        auto rpc = new TestRpc();
        auto reqHdr = rpc->requestPayload.emplaceAppend<WireFormat::Get::Request>();
        reqHdr->common.opcode = WireFormat::GET;
        reqHdr->key = key;
        return rpc;
    }

    TestRpc *putRpc(uint64_t key, std::string value) {
        auto rpc = new TestRpc();
        auto reqHdr = rpc->requestPayload.emplaceAppend<WireFormat::Put::Request>();
        reqHdr->common.opcode = WireFormat::PUT;
        reqHdr->key = key;
        reqHdr->length = value.length();
        rpc->requestPayload.append(value.c_str(), value.length());
        return rpc;
    }

    TestRpc *eraseRpc(uint64_t key) {
        auto rpc = new TestRpc();
        auto reqHdr = rpc->requestPayload.emplaceAppend<WireFormat::Erase::Request>();
        reqHdr->common.opcode = WireFormat::ERASE;
        reqHdr->key = key;
        return rpc;
    }

    void put(uint64_t key, std::string value) {
        TestRpc *rpc = putRpc(key, value);
        Service *service = Service::dispatch(worker, context, rpc);
        worker->schedule(service);
    }

    TestRpc *get(uint64_t key) {
        TestRpc *rpc = getRpc(key);
        Service *service = Service::dispatch(worker, context, rpc);
        worker->schedule(service);
        return rpc;
    }

    std::string getResult(TestRpc *rpc) {
        auto respHdr = rpc->replyPayload.getStart<WireFormat::Get::Response>();
        if (respHdr->common.status == STATUS_OBJECT_DOESNT_EXIST)
            return DOESNT_EXISTS;
        return std::string(rpc->replyPayload.getOffset<char>(sizeof(*respHdr)), respHdr->length);
    }


    TestRpc *erase(uint64_t key) {
        TestRpc *rpc = eraseRpc(key);
        Service *service = Service::dispatch(worker, context, rpc);
        worker->schedule(service);
    }
};

TEST_F(ConcurrentSkipListTest, insertFindRemove) {


    put(14, "a");
    put(4, "b");
    put(12, "abc");
    put(5, "b");
    put(12, "ac");

    while (!worker->isIdle())
        worker->performTask();

    TestRpc *r1 = get(12);
    TestRpc *r2 = get(5);

    while (!worker->isIdle())
        worker->performTask();

    std::string actual = getResult(r1);
    EXPECT_EQ(actual, "ac");
    actual = getResult(r2);
    EXPECT_EQ(actual, "b");
    erase(5);
    while (!worker->isIdle())
        worker->performTask();
    TestRpc *r3 = get(5);
    while (!worker->isIdle())
        worker->performTask();
    actual = getResult(r3);
    EXPECT_EQ(actual, DOESNT_EXISTS);
    TestRpc *r4 = get(14);
    while (!worker->isIdle())
        worker->performTask();
    actual = getResult(r4);
    EXPECT_NE(actual, DOESNT_EXISTS);
}

TEST_F(ConcurrentSkipListTest, iterate) {
    std::set<uint64_t> orderedSet;

    std::vector<int> insertSequence{13, 1, 23, 4, 372, 123};

    for (int key: insertSequence) {
        orderedSet.insert(key);
    }

}

}
