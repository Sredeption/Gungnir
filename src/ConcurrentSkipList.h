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

    enum : int {
        MAX_HEIGHT = 24
    };

    typedef std::unique_lock<SpinLock> ScopedLocker;

    static bool greater(const Key &data, const Node *node);

    static bool less(const Key &data, const Node *node);

    std::allocator<std::atomic<Node *>> allocator;
    std::atomic<Node *> head;
    std::atomic<size_t> size;

    static int findInsertionPoint(
        Node *currentNode, int currentLayer, const Key &key,
        Node *predecessors[], Node *successors[]);

    Node *create(uint8_t height, Key key, bool isHead = false) {
        return new Node(allocator, height, key, isHead);
    }

    void destroy(Node *node) {
        delete node;
    }

    size_t getSize() const;

    int getHeight() const;

    int maxLayer() const;

    size_t incrementSize(int delta);

    // Returns the node if found, nullptr otherwise.
    Node *find(const Key &key) {
        auto ret = findNode(key);
        if (ret.second && !ret.first->markedForRemoval()) {
            return ret.first;
        }
        return nullptr;
    }

    static bool lockNodesForChange(
        int nodeHeight,
        ScopedLocker guards[MAX_HEIGHT],
        Node *predecessors[MAX_HEIGHT],
        Node *successors[MAX_HEIGHT],
        bool adding = true);


    // Returns a paired value:
    //   pair.first always stores the pointer to the node with the same input key.
    //     It could be either the newly added data, or the existed data in the
    //     list with the same key.
    //   pair.second stores whether the data is added successfully:
    //     0 means not added, otherwise reutrns the new size.
    std::pair<Node *, size_t> addOrGetData(Key &key) {
        Node *predecessors[MAX_HEIGHT], *successors[MAX_HEIGHT];
        Node *newNode;
        size_t newSize;
        while (true) {
            int maxMayer = 0;
            int layer = findInsertionPointGetMaxLayer(key, predecessors, successors, &maxMayer);

            if (layer >= 0) {
                Node *nodeFound = successors[layer];
                assert(nodeFound != nullptr);
                if (nodeFound->markedForRemoval()) {
                    continue; // if it's getting deleted retry finding node.
                }
                // wait until fully linked.
                while (!nodeFound->fullyLinked()) {
                }
                return std::make_pair(nodeFound, 0);
            }

            // need to capped at the original height -- the real height may have grown
            int nodeHeight =
                RandomHeight::instance()->getHeight(maxMayer + 1);

            ScopedLocker guards[MAX_HEIGHT];
            if (!lockNodesForChange(nodeHeight, guards, predecessors, successors)) {
                continue; // give up the locks and retry until all valid
            }

            // locks acquired and all valid, need to modify the links under the locks.

            newNode = create(nodeHeight, key);
            for (int k = 0; k < nodeHeight; ++k) {
                newNode->setSkip(k, successors[k]);
                predecessors[k]->setSkip(k, newNode);
            }

            newNode->setFullyLinked();
            newSize = incrementSize(1);
            break;
        }

        int hgt = getHeight();
        size_t sizeLimit =
            RandomHeight::instance()->getSizeLimit(hgt);

        if (hgt < MAX_HEIGHT && newSize > sizeLimit) {
            growHeight(hgt + 1);
        }
        assert(newSize > 0);
        return std::make_pair(newNode, newSize);
    }

    bool remove(const Key &key) {
        Node *nodeToDelete = nullptr;
        ScopedLocker nodeGuard;
        bool isMarked = false;
        int nodeHeight = 0;
        Node *predecessors[MAX_HEIGHT], *successors[MAX_HEIGHT];

        while (true) {
            int max_layer = 0;
            int layer = findInsertionPointGetMaxLayer(key, predecessors, successors, &max_layer);
            if (!isMarked && (layer < 0 || !okToDelete(successors[layer], layer))) {
                return false;
            }

            if (!isMarked) {
                nodeToDelete = successors[layer];
                nodeHeight = nodeToDelete->getHeight();
                nodeGuard = nodeToDelete->acquireGuard();
                if (nodeToDelete->markedForRemoval()) {
                    return false;
                }
                nodeToDelete->setMarkedForRemoval();
                isMarked = true;
            }

            // acquire pred locks from bottom layer up
            ScopedLocker guards[MAX_HEIGHT];
            if (!lockNodesForChange(nodeHeight, guards, predecessors, successors, false)) {
                continue; // this will unlock all the locks
            }

            for (int k = nodeHeight - 1; k >= 0; --k) {
                predecessors[k]->setSkip(k, nodeToDelete->skip(k));
            }

            incrementSize(-1);
            break;
        }
        destroy(nodeToDelete);
        return true;
    }

    const Key *first() const {
        auto node = head.load(std::memory_order_consume)->skip(0);
        return node ? &node->getKey() : nullptr;
    }

    const Key *last() const {
        Node *pred = head.load(std::memory_order_consume);
        Node *node = nullptr;
        for (int layer = maxLayer(); layer >= 0; --layer) {
            do {
                node = pred->skip(layer);
                if (node) {
                    pred = node;
                }
            } while (node != nullptr);
        }
        return pred == head.load(std::memory_order_relaxed) ? nullptr : &pred->getKey();
    }

    static bool okToDelete(Node *candidate, int layer) {
        assert(candidate != nullptr);
        return candidate->fullyLinked() && candidate->maxLayer() == layer &&
               !candidate->markedForRemoval();
    }

    // find node for insertion/deleting
    int findInsertionPointGetMaxLayer(
        const Key &data,
        Node *predecessors[],
        Node *successors[],
        int *max_layer) const {
        *max_layer = maxLayer();
        return findInsertionPoint(
            head.load(std::memory_order_consume), *max_layer, data, predecessors, successors);
    }

    // Find node for access. Returns a paired values:
    // pair.first = the first node that no-less than data value
    // pair.second = 1 when the data value is founded, or 0 otherwise.
    // This is like lowerBound, but not exact: we could have the node marked for
    // removal so still need to check that.
    std::pair<Node *, int> findNode(const Key &key) const {
        return findNodeDownRight(key);
    }

    // Find node by first stepping down then stepping right. Based on benchmark
    // results, this is slightly faster than findNodeRightDown for better
    // localality on the skipping pointers.
    std::pair<Node *, int> findNodeDownRight(const Key &data) const {
        Node *pred = head.load(std::memory_order_consume);
        int ht = pred->getHeight();
        Node *node = nullptr;

        bool found = false;
        while (!found) {
            // stepping down
            for (; ht > 0 && less(data, node = pred->skip(ht - 1)); --ht) {
            }
            if (ht == 0) {
                return std::make_pair(node, 0); // not found
            }
            // node <= data now, but we need to fix up ht
            --ht;

            // stepping right
            while (greater(data, node)) {
                pred = node;
                node = node->skip(ht);
            }
            found = !less(data, node);
        }
        return std::make_pair(node, found);
    }

    // find node by first stepping right then stepping down.
    // We still keep this for reference purposes.
    std::pair<Node *, int> findNodeRightDown(const Key &key) const {
        Node *pred = head.load(std::memory_order_consume);
        Node *node = nullptr;
        auto top = maxLayer();
        int found = 0;
        for (int layer = top; !found && layer >= 0; --layer) {
            node = pred->skip(layer);
            while (greater(key, node)) {
                pred = node;
                node = node->skip(layer);
            }
            found = !less(key, node);
        }
        return std::make_pair(node, found);
    }

    Node *lowerBound(const Key &data) const {
        auto node = findNode(data).first;
        while (node != nullptr && node->markedForRemoval()) {
            node = node->skip(0);
        }
        return node;
    }


    void growHeight(int height) {
        Node *oldHead = head.load(std::memory_order_consume);
        if (oldHead->getHeight() >= height) { // someone else already did this
            return;
        }

        Node *newHead = create(height, Key(), true);

        { // need to guard the head node in case others are adding/removing
            // nodes linked to the head.
            ScopedLocker g = oldHead->acquireGuard();
            newHead->copyHead(oldHead);
            Node *expected = oldHead;
            if (!head.compare_exchange_strong(
                expected, newHead, std::memory_order_release)) {
                // if someone has already done the swap, just return.
                destroy(newHead);
                return;
            }
            oldHead->setMarkedForRemoval();
        }
        destroy(oldHead);
    }

public:


    class Iterator {

    };

    class Accessor {
    public:

    };


};

}

#endif //GUNGNIR_CONCURRENTSKIPLIST_H
