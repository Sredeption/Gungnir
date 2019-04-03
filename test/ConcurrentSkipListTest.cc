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

TEST_F(ConcurrentSkipListTest, insertFindRemove) {

    ConcurrentSkipList::Accessor accessor(&skipList);
    std::set<uint64_t> orderedSet;

    accessor.insert(12);
    accessor.insert(14);
    accessor.insert(4);

    auto iter = accessor.find(12);

    EXPECT_EQ(iter.getKey().value(), 12);
    accessor.remove(12);
    EXPECT_EQ(accessor.find(12), accessor.end());
    EXPECT_TRUE(accessor.find(4).good());
}

TEST_F(ConcurrentSkipListTest, iterate) {
    ConcurrentSkipList::Accessor accessor(&skipList);
    std::set<uint64_t> orderedSet;

    std::vector<int> insertSequence{13, 1, 23, 4, 372, 123};

    for (int key: insertSequence) {
        orderedSet.insert(key);
        accessor.insert(key);
    }

    EXPECT_EQ(orderedSet.find(2), orderedSet.end());

    auto iterator = accessor.begin();
    for (uint64_t expected : orderedSet) {
        uint64_t actual = iterator.getKey().value();
        EXPECT_EQ(actual, expected);
        iterator.next();
    }

}

}
