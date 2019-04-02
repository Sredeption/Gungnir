#include <cassert>
#include "ConcurrentSkipList.h"

namespace Gungnir {

ConcurrentSkipList::Node::Node(std::allocator<std::atomic<ConcurrentSkipList::Node *>> &allocator, uint8_t height,
                               Key key, bool isHead)
    : allocator(allocator), flags(), height(height), key(std::forward<Key>(key)), spinLock("SkipList::Node") {
    setFlags(0);
    if (isHead) {
        setIsHeadNode();
    }
    // need to explicitly init the dynamic atomic pointer array
    forward = allocator.allocate(height);
    for (int i = 0; i < height; i++) {
        allocator.construct(forward + i, nullptr);
    }
}

ConcurrentSkipList::Node::~Node() {
    allocator.deallocate(forward, height);
}

// copy the head node to a new head node assuming lock acquired
ConcurrentSkipList::Node *ConcurrentSkipList::Node::copyHead(Node *node) {
    assert(node != nullptr && height > node->height);
    setFlags(node->getFlags());
    for (uint8_t i = 0; i < node->height; ++i) {
        setSkip(i, node->skip(i));
    }
    return this;
}

ConcurrentSkipList::Node *ConcurrentSkipList::Node::skip(int layer) const {
    assert(layer < height);
    return forward[layer].load(std::memory_order_consume);
}

// next valid node as in the linked list
ConcurrentSkipList::Node *ConcurrentSkipList::Node::next() {
    Node *node;
    for (node = skip(0); (node != nullptr && node->markedForRemoval());
         node = node->skip(0)) {
    }
    return node;
}

void ConcurrentSkipList::Node::setSkip(uint8_t h, Node *next) {
    assert(h < height);
    forward[h].store(next, std::memory_order_release);
}

Key &ConcurrentSkipList::Node::getKey() {
    return key;
}

const Key &ConcurrentSkipList::Node::getKey() const {
    return key;
}

int ConcurrentSkipList::Node::maxLayer() const {
    return height - 1;
}

int ConcurrentSkipList::Node::getHeight() const {
    return height;
}

std::unique_lock<SpinLock> ConcurrentSkipList::Node::acquireGuard() {
    return std::unique_lock<SpinLock>(spinLock);
}

bool ConcurrentSkipList::Node::fullyLinked() const {
    return getFlags() & FULLY_LINKED;
}

bool ConcurrentSkipList::Node::isHeadNode() const {
    return getFlags() & IS_HEAD_NODE;
}

void ConcurrentSkipList::Node::setIsHeadNode() {
    setFlags(uint16_t(getFlags() | IS_HEAD_NODE));
}

void ConcurrentSkipList::Node::setFullyLinked() {
    setFlags(uint16_t(getFlags() | FULLY_LINKED));
}

void ConcurrentSkipList::Node::setMarkedForRemoval() {
    setFlags(uint16_t(getFlags() | MARKED_FOR_REMOVAL));
}

ConcurrentSkipList::RandomHeight *ConcurrentSkipList::RandomHeight::instance() {
    static RandomHeight instance_;
    return &instance_;
}

int ConcurrentSkipList::RandomHeight::getHeight(int maxHeight) const {
    assert(maxHeight < MaxHeightLimit);
    double p = randomProb();
    for (int i = 0; i < maxHeight; ++i) {
        if (p < lookupTable[i]) {
            return i + 1;
        }
    }
    return maxHeight;
}

size_t ConcurrentSkipList::RandomHeight::getSizeLimit(int height) const {
    assert(height < MaxHeightLimit);
    return sizeLimitTable[height];
}

ConcurrentSkipList::RandomHeight::RandomHeight() : lookupTable(), sizeLimitTable() {
    initLookupTable();
}

void ConcurrentSkipList::RandomHeight::initLookupTable() {
    // set skip prob = 1/E
    static const double probInv = std::exp(1);
    static const double prob = 1.0 / probInv;
    static const size_t maxSizeLimit = std::numeric_limits<size_t>::max();

    double sizeLimit = 1;
    double p = lookupTable[0] = (1 - prob);
    sizeLimitTable[0] = 1;
    for (int i = 1; i < MaxHeightLimit - 1; ++i) {
        p *= prob;
        sizeLimit *= probInv;
        lookupTable[i] = lookupTable[i - 1] + p;
        sizeLimitTable[i] = sizeLimit > maxSizeLimit
                            ? maxSizeLimit
                            : static_cast<size_t>(sizeLimit);
    }
    lookupTable[MaxHeightLimit - 1] = 1;
    sizeLimitTable[MaxHeightLimit - 1] = maxSizeLimit;
}

double ConcurrentSkipList::RandomHeight::randomProb() {
    static thread_local boost::lagged_fibonacci2281 rng_;
    return rng_();
}

bool ConcurrentSkipList::greater(const Key &data, const Node *node) {
    return node;
}

bool ConcurrentSkipList::less(const Key &data, const Node *node) {
    return node == nullptr;
}

int ConcurrentSkipList::findInsertionPoint(Node *currentNode, int currentLayer, const Key &key,
                                           Node *predecessors[], Node *successors[]) {

    int foundLayer = -1;
    Node *predecessor = currentNode;
    Node *foundNode = nullptr;
    for (int layer = currentLayer; layer >= 0; --layer) {
        Node *node = predecessor->skip(layer);
        while (greater(key, node)) {
            predecessor = node;
            node = node->skip(layer);
        }
        if (foundLayer == -1 && !less(key, node)) { // the two keys equal
            foundLayer = layer;
            foundNode = node;
        }
        predecessors[layer] = predecessor;

        // if found, succs[0..foundLayer] need to point to the cached foundNode,
        // as foundNode might be deleted at the same time thus pred->skip() can
        // return nullptr or another node.
        successors[layer] = foundNode ? foundNode : node;
    }
    return foundLayer;
}

size_t ConcurrentSkipList::getSize() const {
    return size.load(std::memory_order_relaxed);
}

int ConcurrentSkipList::getHeight() const {
    return head.load(std::memory_order_consume)->getHeight();
}

int ConcurrentSkipList::maxLayer() const {
    return getHeight() - 1;
}

size_t ConcurrentSkipList::incrementSize(int delta) {
    return size.fetch_add(delta, std::memory_order_relaxed) + delta;
}

bool ConcurrentSkipList::lockNodesForChange(int nodeHeight, ConcurrentSkipList::ScopedLocker *guards,
                                            ConcurrentSkipList::Node **predecessors,
                                            ConcurrentSkipList::Node **successors, bool adding) {
    Node *predecessor, *successor, *prevPred = nullptr;
    bool valid = true;
    for (int layer = 0; valid && layer < nodeHeight; ++layer) {
        predecessor = predecessors[layer];
        assert(predecessor != nullptr);
        successor = successors[layer];
        if (predecessor != prevPred) {
            guards[layer] = predecessor->acquireGuard();
            prevPred = predecessor;
        }
        valid = !predecessor->markedForRemoval() &&
                predecessor->skip(layer) == successor; // check again after locking

        if (adding) { // when adding a node, the successor shouldn't be going away
            valid = valid && (successor == nullptr || !successor->markedForRemoval());
        }
    }

    return valid;
}
}
