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

    Node *create(uint8_t height, Key key, bool isHead = false);

    void destroy(Node *node);

    size_t getSize() const;

    int getHeight() const;

    int maxLayer() const;

    size_t incrementSize(int delta);

    // Returns the node if found, nullptr otherwise.
    Node *find(const Key &key);

    static bool lockNodesForChange(
        int nodeHeight,
        ScopedLocker guards[MAX_HEIGHT],
        Node *predecessors[MAX_HEIGHT],
        Node *successors[MAX_HEIGHT],
        bool adding = true);

    std::pair<Node *, size_t> addOrGetData(const Key &key);

    bool remove(const Key &key);

    const Key *first() const;

    const Key *last() const;

    static bool okToDelete(Node *candidate, int layer);

    int findInsertionPointGetMaxLayer(
        const Key &data,
        Node *predecessors[],
        Node *successors[],
        int *max_layer) const;

    std::pair<Node *, int> findNode(const Key &key) const;

    std::pair<Node *, int> findNodeDownRight(const Key &data) const;

    std::pair<Node *, int> findNodeRightDown(const Key &key) const;

    Node *lowerBound(const Key &data) const;

    void growHeight(int height);

public:

    explicit ConcurrentSkipList(int height = 16);

    class Iterator {
    public:
        explicit Iterator(Node *node = nullptr) : node(node) {}

        Iterator(Iterator &other);

        Iterator(Iterator &&other) noexcept;

        bool good() const;

        void next();

        bool isDone();

        Key getKey();

        bool operator==(const Iterator &that) const;

    private:
        Node *node;
    };

    class Accessor {
    public:
        explicit Accessor(ConcurrentSkipList *skipList);

        Accessor(const Accessor &) = delete;

        Accessor &operator=(const Accessor &) = delete;

        bool empty() const;

        size_t size() const;

        // returns end() if the value is not in the list, otherwise returns an
        // iterator pointing to the data, and it's guaranteed that the data is valid
        // as far as the Accessor is hold.
        Iterator find(const Key &value);


        Iterator begin() const;

        Iterator end() const;

        Iterator insert(Key data);

        size_t erase(const Key &data);

        Iterator lowerBound(const Key &data) const;

        size_t height() const;

        const Key *first() const;

        const Key *last() const;

        bool popBack();

        std::pair<Key *, bool> addOrGetData(const Key &data);

        bool contains(const Key &key) const;

        bool add(const Key &key);

        bool remove(const Key &key);


    private:
        ConcurrentSkipList *skipList;

    };


};

}

#endif //GUNGNIR_CONCURRENTSKIPLIST_H
