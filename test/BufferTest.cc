
#include <gtest/gtest.h>
#include "Buffer.h"

namespace Gungnir {

class BufferTest : public ::testing::Test {

};

TEST_F(BufferTest, getRangeBasics) {
    Buffer buffer;
    const char *chunk = "0123456789";
    buffer.append("abcde", 5);
    buffer.append(chunk, 10);
    char *result = static_cast<char *>(buffer.getRange(8, 3));
    EXPECT_EQ(buffer.size(), 15);
    EXPECT_EQ('3', *result);
    EXPECT_EQ('6', result[3]);
}

TEST_F(BufferTest, getRangeEmptyBuffer) {
    Buffer buffer;
    void *result = buffer.getRange(0, 0);
    EXPECT_EQ(nullptr, result);
}

TEST_F(BufferTest, peekOutOfRange) {
    Buffer buffer;
    buffer.append("abcde", 5);
    void *pointer;
    uint32_t length = buffer.peek(5, &pointer);
    EXPECT_EQ(nullptr, pointer);
    EXPECT_EQ(0u, length);
}

TEST_F(BufferTest, peek_searchFromStart) {
    Buffer buffer;
    buffer.append("abcde", 5);
    buffer.append("0123456789", 10);
    buffer.append("ABCDEF", 6);
    char *pointer;
    uint32_t length = buffer.peek(17, reinterpret_cast<void **>(&pointer));
    EXPECT_EQ('C', *pointer);
    EXPECT_EQ(4u, length);
}
}

