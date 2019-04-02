#ifndef GUNGNIR_CONCURRENTSKIPLIST_H
#define GUNGNIR_CONCURRENTSKIPLIST_H

#include <cstdint>
#include <atomic>
#include <memory>
#include <cmath>


#include <boost/random.hpp>

#include "Key.h"
#include "Value.h"
#include "SpinLock.h"


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
        Node(std::allocator<std::atomic<Node *>> &allocator, uint8_t height, Key key, bool isHead);

        ~Node();

        Node(const Node &) = delete;

        Node &operator=(const Node &) = delete;

        Node *copyHead(Node *node);

        Node *skip(int layer) const;

        Node *next();

        void setSkip(uint8_t h, Node *next);

        Key &getKey();

        const Key &getKey() const;

        int maxLayer() const;

        int getHeight() const;

        std::unique_lock<SpinLock> acquireGuard();

        bool fullyLinked() const;

        bool markedForRemoval() const {
            return getFlags() & MARKED_FOR_REMOVAL;
        }

        bool isHeadNode() const;

        void setIsHeadNode();

        void setFullyLinked();

        void setMarkedForRemoval();

    private:

        uint16_t getFlags() const {
            return flags.load(std::memory_order_consume);
        }

        void setFlags(uint16_t flags) {
            this->flags.store(flags, std::memory_order_release);
        }

        std::allocator<std::atomic<Node *>> &allocator;
        Key key;
        Value value;
        std::atomic<uint16_t> flags;
        const uint8_t height;
        SpinLock spinLock;
        std::atomic<Node *> *forward;

    };

    class RandomHeight {
    private:
        enum {
            MaxHeightLimit = 64
        };
    public:
        static RandomHeight *instance();

        int getHeight(int maxHeight) const;

        size_t getSizeLimit(int height) const;

    private:
        RandomHeight();

        void initLookupTable();

        static double randomProb();

        double lookupTable[MaxHeightLimit];
        size_t sizeLimitTable[MaxHeightLimit];
    };

    static bool greater(const Key &data, const Node *node);

    static bool less(const Key &data, const Node *node);

    static int findInsertionPoint(
        Node *currentNode, int currentLayer, const Key &key,
        Node *predecessors[], Node *successors[]);

public:


    class Iterator {

    };

    class Accessor {
    public:

    };


};

}

#endif //GUNGNIR_CONCURRENTSKIPLIST_H
