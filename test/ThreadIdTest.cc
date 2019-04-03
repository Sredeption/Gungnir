#include <gtest/gtest.h>
#include <thread>
#include "ThreadId.h"

namespace Gungnir {
class ThreadIdTest : public ::testing::Test {
public:
    ThreadIdTest() = default;

};

// Helper function that runs in a separate thread.  It reads its id and
// saves it in the variable pointed to by its argument.
static void readThreadId(int *p) {
    *p = ThreadId::get();
}

TEST_F(ThreadIdTest, basics) {
    int value;
    EXPECT_EQ(1, ThreadId::get());
    EXPECT_EQ(1, ThreadId::get());
    std::thread thread1(readThreadId, &value);
    thread1.join();
    EXPECT_EQ(2, value);
    std::thread thread2(readThreadId, &value);
    thread2.join();
    EXPECT_EQ(3, value);
    EXPECT_EQ(1, ThreadId::get());
}
}
