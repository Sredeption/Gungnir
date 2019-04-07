#include <cassert>
#include "ConcurrentSkipList.h"
#include "LogCleaner.h"

namespace Gungnir {

ConcurrentSkipList::Node::Node(std::allocator<std::atomic<ConcurrentSkipList::Node *>> &allocator, uint8_t height,
                               Key key, bool isHead)
    : allocator(allocator), key(std::forward<Key>(key)), flags(), height(height), spinLock()
      , forward(), object() {
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
    for (int i = 0; i < height; i++) {
        allocator.destroy(forward + i);
    }
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

std::unique_lock<SpinLock> ConcurrentSkipList::Node::tryAcquireGuard() {
    return std::unique_lock<SpinLock>(spinLock, std::try_to_lock);
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

Object *ConcurrentSkipList::Node::setObject(Object *object) {
    Object *old = this->object;
    this->object = object;
    return old;
}

Object *ConcurrentSkipList::Node::getObject() {
    return this->object;
}

ConcurrentSkipList::RandomHeight *ConcurrentSkipList::RandomHeight::instance() {
    static RandomHeight instance;
    return &instance;
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
    return node && data.value() > node->getKey().value();
}

bool ConcurrentSkipList::less(const Key &data, const Node *node) {
    return node == nullptr || data.value() < node->getKey().value();
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

ConcurrentSkipList::Node *ConcurrentSkipList::create(uint8_t height, Key key, bool isHead) {
    return new Node(allocator, height, key, isHead);
}


void ConcurrentSkipList::destroy(ConcurrentSkipList::Node *node) {
    if (node != nullptr) {
        int removalEpoch = epoch.fetch_add(1);
        context->logCleaner->collect(removalEpoch, node);
    }
}

void ConcurrentSkipList::destroy(Object *object) {
    if (object != nullptr) {
        int removalEpoch = epoch.fetch_add(1);
        context->logCleaner->collect(removalEpoch, object);
    }
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

ConcurrentSkipList::Node *ConcurrentSkipList::find(const Key &key) {
    auto ret = findNode(key);
    if (ret.second && !ret.first->markedForRemoval()) {
        return ret.first;
    }
    return nullptr;
}


bool ConcurrentSkipList::tryLockNodesForChange(int nodeHeight, LayerLocker guards,
                                               Node **predecessors,
                                               Node **successors, bool adding) {
    Node *predecessor, *successor, *prevPred = nullptr;
    bool valid = true;
    for (int layer = 0; valid && layer < nodeHeight; ++layer) {
        predecessor = predecessors[layer];
        assert(predecessor != nullptr);
        successor = successors[layer];
        if (predecessor != prevPred) {
            guards[layer] = predecessor->tryAcquireGuard();
            if (!guards[layer].owns_lock()) {
                for (int i = 0; i < layer; i++) {
                    guards[layer].unlock();
                }
                return false;
            }
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

ConcurrentSkipList::Node *ConcurrentSkipList::addOrGetNode(const Key &key) {
    Node *predecessors[MAX_HEIGHT], *successors[MAX_HEIGHT];
    Node *newNode = nullptr;
    size_t newSize;
    int maxMayer = 0;
    int layer = findInsertionPointGetMaxLayer(key, predecessors, successors, &maxMayer);

    if (layer >= 0) {
        Node *nodeFound = successors[layer];
        assert(nodeFound != nullptr);
        if (nodeFound->markedForRemoval()) {
            return nullptr; // if it's getting deleted retry finding node.
        }
        // wait until fully linked.
        while (!nodeFound->fullyLinked()) {
        }
        return nodeFound;
    }

    // need to capped at the original height -- the real height may have grown
    int nodeHeight = RandomHeight::instance()->getHeight(maxMayer + 1);

    LayerLocker guards;
    bool locked = false;
    for (int i = 0; i < 10; i++) {
        locked = tryLockNodesForChange(nodeHeight, guards, predecessors, successors);
        if (locked) {
            break;
        }
    }
    if (!locked)
        return nullptr;

    // locks acquired and all valid, need to modify the links under the locks.

    newNode = create(nodeHeight, key);
    for (int k = 0; k < nodeHeight; ++k) {
        newNode->setSkip(k, successors[k]);
        predecessors[k]->setSkip(k, newNode);
    }

    newNode->setFullyLinked();
    newSize = incrementSize(1);

    assert(newSize > 0);
    return newNode;
}

bool ConcurrentSkipList::remove(const Key &key) {
    Node *nodeToDelete = nullptr;
    ScopedLocker nodeGuard;
    bool isMarked = false;
    int nodeHeight = 0;
    Node *predecessors[MAX_HEIGHT], *successors[MAX_HEIGHT];

    while (true) {
        int maxLayer = 0;
        int layer = findInsertionPointGetMaxLayer(key, predecessors, successors, &maxLayer);
        if (!isMarked && (layer < 0 || !okToDelete(successors[layer], layer))) {
            return false;
        }

        if (!isMarked) {
            nodeToDelete = successors[layer];
            nodeHeight = nodeToDelete->getHeight();
            nodeGuard = nodeToDelete->tryAcquireGuard();
            if (nodeToDelete->markedForRemoval()) {
                return false;
            }
            nodeToDelete->setMarkedForRemoval();
            isMarked = true;
        }

        // acquire pred locks from bottom layer up
        LayerLocker guards;
        if (!tryLockNodesForChange(nodeHeight, guards, predecessors, successors, false)) {
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

const Key *ConcurrentSkipList::first() const {
    auto node = head.load(std::memory_order_consume)->skip(0);
    return node ? &node->getKey() : nullptr;
}

const Key *ConcurrentSkipList::last() const {
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

bool ConcurrentSkipList::okToDelete(ConcurrentSkipList::Node *candidate, int layer) {
    assert(candidate != nullptr);
    return candidate->fullyLinked() && candidate->maxLayer() == layer &&
           !candidate->markedForRemoval();
}

// find node for insertion/deleting
int ConcurrentSkipList::findInsertionPointGetMaxLayer(const Key &data, ConcurrentSkipList::Node **predecessors,
                                                      ConcurrentSkipList::Node **successors, int *max_layer) const {
    *max_layer = maxLayer();
    return findInsertionPoint(
        head.load(std::memory_order_consume), *max_layer, data, predecessors, successors);
}

// Find node for access. Returns a paired values:
// pair.first = the first node that no-less than data value
// pair.second = 1 when the data value is founded, or 0 otherwise.
// This is like lowerBound, but not exact: we could have the node marked for
// removal so still need to check that.
std::pair<ConcurrentSkipList::Node *, int> ConcurrentSkipList::findNode(const Key &key) const {
    return findNodeDownRight(key);
}

// Find node by first stepping down then stepping right. Based on benchmark
// results, this is slightly faster than findNodeRightDown for better
// localality on the skipping pointers.
std::pair<ConcurrentSkipList::Node *, int> ConcurrentSkipList::findNodeDownRight(const Key &data) const {
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
std::pair<ConcurrentSkipList::Node *, int> ConcurrentSkipList::findNodeRightDown(const Key &key) const {
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

ConcurrentSkipList::Node *ConcurrentSkipList::lowerBound(const Key &data) const {
    auto node = findNode(data).first;
    while (node != nullptr && node->markedForRemoval()) {
        node = node->skip(0);
    }
    return node;
}

ConcurrentSkipList::ConcurrentSkipList(Context *context, int height)
    : context(context), allocator(), head(create(height, Key(), true)), size(0), epoch(0) {

}

ConcurrentSkipList::Iterator::Iterator(Iterator &other) {
    node = other.node;
}

ConcurrentSkipList::Iterator::Iterator(Iterator &&other) noexcept {
    node = other.node;
}

bool ConcurrentSkipList::Iterator::good() const {
    return node != nullptr;
}

void ConcurrentSkipList::Iterator::next() {
    node = node->next();
}

bool ConcurrentSkipList::Iterator::isDone() {
    return node == nullptr;
}

Key ConcurrentSkipList::Iterator::getKey() {
    return node->getKey();
}

bool ConcurrentSkipList::Iterator::operator==(const Iterator &that) const {
    return this->node == that.node;
}

}
