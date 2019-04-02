#include <gtest/gtest.h>
#include <set>
#include <Logger.h>

#include "ConcurrentSkipList.h"

namespace Gungnir {

struct ConcurrentSkipListTest : public ::testing::Test {
    ConcurrentSkipList skipList;

    ConcurrentSkipListTest()
        : skipList() {

    }

};


TEST_F(ConcurrentSkipListTest, basicTest) {
    ConcurrentSkipList::Accessor accessor(&skipList);
    std::set<uint64_t> orderedSet;

    std::vector<uint64_t> insertSequence = {13, 1, 23, 4, 372, 123};

    for (uint64_t key: insertSequence) {
        orderedSet.insert(key);
        accessor.insert(key);
    }

    Logger::log(HERE, "start");
    auto iterator = accessor.begin();
    for (uint64_t expected : orderedSet) {
        EXPECT_EQ(iterator.getKey().value(), expected);
        Logger::log(HERE, "actual:%lu, expected:%lu", expected);
        iterator.next();
    }

}

}
