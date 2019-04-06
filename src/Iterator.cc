#include "Iterator.h"

namespace Gungnir {

Iterator::Iterator() : Iterator(new Buffer()) {

}

Iterator::Iterator(Buffer *buffer) : buffer(buffer), size(0), offset(0) {

}

Iterator::~Iterator() {
    delete buffer;
}

uint64_t Iterator::getKey() {
    auto *key = buffer->getOffset<uint64_t>(offset);
    return *key;
}

void *Iterator::getValue(uint32_t &length) {
    auto *size = buffer->getOffset<uint32_t>(offset + 4);
    length = *size;
    return buffer->getRange(offset + 12, length);
}

void Iterator::next() {
    auto *size = buffer->getOffset<uint32_t>(offset + 4);
    offset += 12u + *size;
}

bool Iterator::isDone() {
    return offset >= buffer->size();
}
}
