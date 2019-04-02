#ifndef GUNGNIR_CONCURRENTSKIPLIST_H
#define GUNGNIR_CONCURRENTSKIPLIST_H

#include <cstdint>

#include "Key.h"
#include "Value.h"


namespace Gungnir {

class ConcurrentSkipList {
private:

    class Node {
        enum : uint16_t {
            IS_HEAD_NODE = 1U,
            MARKED_FOR_REMOVAL = (1U << 1),
            FULLY_LINKED = (1U << 2),
        };

    public:
        Node(const Node &) = delete;

        Node &operator=(const Node &) = delete;

    };

public:


};

}

#endif //GUNGNIR_CONCURRENTSKIPLIST_H
