#include <gtest/gtest.h>
#include "Logger.h"
#include "Object.h"
#include "Log.h"

namespace Gungnir {

struct LogTest : public ::testing::Test {

public:
    Log *log;
    const char *filePath = "/tmp/log-test";
    int segmentSize = 500;

    LogTest() : log(nullptr) {
    }

};

std::string toString(Buffer *buffer) {
    return std::string(buffer->getStart<char>(), buffer->size());
}


TEST_F(LogTest, writeAndRead) {
    log = new Log(filePath, false, segmentSize);

    std::string data;
    for (int i = 0; i < 100; i++) {
        data = std::to_string(i * 2);
        LogEntry *entry;

        if (i % 2 == 0) {
            entry = new Object(i, data.c_str(), data.length());
        } else {
            entry = new ObjectTombstone(i);
        }
        log->append(entry);
    }
    while (log->write());
    delete log;


    log = new Log(filePath, true, segmentSize);
    for (int i = 0; i < 100; i++) {
        LogEntry *entry = log->read();
        LogEntryType expectType;
        if (i % 2 == 0) {
            auto *object = dynamic_cast<Object *>(entry);
            expectType = LOG_ENTRY_TYPE_OBJ;
            std::string actualValue = toString(&object->value);
            EXPECT_EQ(actualValue, std::to_string(i * 2));
        } else
            expectType = LOG_ENTRY_TYPE_OBJTOMB;
        EXPECT_EQ(entry->type, expectType);
        EXPECT_EQ(entry->key.value(), i);
    }

    EXPECT_EQ(log->read(), nullptr);
    delete log;

}

TEST_F(LogTest, writerThread) {
    std::string data;

    log = new Log(filePath, false, 500);
    log->startWriter();
    uint64_t toOffset = 0;
    for (int i = 0; i < 100; i++) {
        data = std::to_string(i * 3);
        LogEntry *entry;

        if (i % 2 == 0) {
            entry = new Object(i, data.c_str(), data.length());
        } else {
            entry = new ObjectTombstone(i);
        }

        toOffset = log->append(entry);
        if (i % 20 == 19) {
            while (!log->sync(toOffset));
        }
    }
    while (!log->sync(toOffset));

    delete log;

    log = new Log(filePath, true, segmentSize);
    for (int i = 0; i < 100; i++) {
        LogEntry *entry = log->read();
        LogEntryType expectType;
        if (i % 2 == 0) {
            auto *object = dynamic_cast<Object *>(entry);
            expectType = LOG_ENTRY_TYPE_OBJ;
            std::string actualValue = toString(&object->value);
            EXPECT_EQ(actualValue, std::to_string(i * 3));
        } else
            expectType = LOG_ENTRY_TYPE_OBJTOMB;
        EXPECT_EQ(entry->type, expectType);
        EXPECT_EQ(entry->key.value(), i);
    }

    EXPECT_EQ(log->read(), nullptr);
    delete log;
}

}
