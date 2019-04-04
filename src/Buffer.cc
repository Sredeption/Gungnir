#include <cstring>
#include <cassert>
#include "Buffer.h"
#include "Exception.h"

namespace Gungnir {

Buffer::Chunk::Chunk(void *data, uint32_t length)
    : next(nullptr)
      , data(static_cast<char *>(data))
      , length(length) {

}

Buffer::Chunk::~Chunk() = default;

Buffer::Buffer() :
    totalLength(0), firstChunk(nullptr), lastChunk(nullptr), cursorChunk(nullptr), cursorOffset(~0u), allocations() {

}

Buffer::~Buffer() {
    resetInternal(false);
}

void *Buffer::alloc(size_t numBytes) {
    uint32_t byteAllocated;
    auto *chunk = new Chunk(getNewAllocation(numBytes, &byteAllocated), numBytes);

    totalLength += numBytes;
    if (lastChunk != nullptr) {
        lastChunk->next = chunk;
    } else {
        firstChunk = chunk;
    }
    lastChunk = chunk;
    return chunk->data;
}

void Buffer::append(const void *data, uint32_t numBytes) {
    memcpy(alloc(numBytes), data, numBytes);
}

void Buffer::append(Buffer *src, uint32_t offset, uint32_t length) {
    Iterator it(src, offset, length);

    while (!it.isDone()) {
        append(it.getData(), it.getLength());
    }
}

uint32_t Buffer::peek(uint32_t offset, void **returnPtr) {
    if (offset >= totalLength) {
        *returnPtr = nullptr;
        return 0;
    }

    // Try the fast path first: is the desired byte in the current chunk?
    uint32_t bytesToSkip;
    Chunk *chunk;
    if (offset >= cursorOffset) {
        bytesToSkip = offset - cursorOffset;
        chunk = cursorChunk;
    } else {
        // The cached info is past the desired point; must start
        // searching at the beginning of the buffer.
        chunk = firstChunk;
        bytesToSkip = offset;
    }

    // Find the chunk containing the first byte of the range.
    while (bytesToSkip >= chunk->length) {
        bytesToSkip -= chunk->length;
        chunk = chunk->next;
    }
    cursorChunk = chunk;
    cursorOffset = offset - bytesToSkip;
    *returnPtr = chunk->data + bytesToSkip;
    return chunk->length - bytesToSkip;
}

void Buffer::reset() {
    resetInternal(true);
}

void Buffer::truncate(uint32_t newLength) {
    uint32_t bytesLeft = newLength;
    if (bytesLeft >= totalLength) {
        return;
    }
    if (bytesLeft == 0) {
        reset();
        return;
    }

    // Find the last chunk that we will retain.
    Chunk *chunk = firstChunk;
    while (bytesLeft > chunk->length) {
        bytesLeft -= chunk->length;
        chunk = chunk->next;
    }

    // Shorten the last chunk, if needed.
    chunk->length = bytesLeft;


    // Shorten the last chunk, if needed.
    chunk->length = bytesLeft;

    // Delete all the chunks after this one.
    lastChunk = chunk;
    chunk = chunk->next;
    lastChunk->next = nullptr;
    while (chunk != nullptr) {
        Chunk *temp = chunk->next;
        delete chunk;
        chunk = temp;
    }
    totalLength = newLength;

    // Flush cached location information.
    if (cursorOffset >= totalLength) {
        cursorOffset = ~0u;
        cursorChunk = nullptr;
    }
}

void Buffer::truncateFront(uint32_t bytesToDelete) {
    if (bytesToDelete >= totalLength) {
        reset();
        return;
    }
    totalLength -= bytesToDelete;
    cursorChunk = nullptr;
    cursorOffset = ~0u;

    // Work through the initial chunks, one at a time, until we've
    // deleted the right number of bytes.
    while (true) {
        Chunk *chunk = firstChunk;
        if (bytesToDelete < chunk->length) {
            chunk->length -= bytesToDelete;
            chunk->data += bytesToDelete;
            break;
        }
        bytesToDelete -= chunk->length;
        firstChunk = chunk->next;
        delete chunk;
    }
}

uint32_t Buffer::write(uint32_t offset, uint32_t length, FILE *f) {
    throw FatalError(HERE, "Not implemented yet");
}

uint32_t Buffer::getNumberChunks() {
    uint32_t count = 0;
    for (Chunk *chunk = firstChunk; chunk != nullptr; chunk = chunk->next) {
        count++;
    }
    return count;
}

uint32_t Buffer::copy(uint32_t offset, uint32_t length, void *dest) {
    if ((offset + length) >= totalLength) {
        if (offset >= totalLength) {
            return 0;
        }
        length = totalLength - offset;
    }

    // Find the chunk containing the first byte to copy.
    if (offset < cursorOffset) {
        cursorOffset = 0;
        cursorChunk = firstChunk;
    }
    while ((offset - cursorOffset) >= cursorChunk->length) {
        cursorOffset += cursorChunk->length;
        cursorChunk = cursorChunk->next;
    }

    // Each iteration through the following loop copies bytes from one chunk.
    char *out = static_cast<char *>(dest);
    uint32_t bytesLeft = length;
    char *chunkData = cursorChunk->data + (offset - cursorOffset);
    uint32_t bytesThisChunk = cursorChunk->length - (offset - cursorOffset);
    while (true) {
        if (bytesThisChunk > bytesLeft) {
            bytesThisChunk = bytesLeft;
        }
        memcpy(out, chunkData, bytesThisChunk);
        out += bytesThisChunk;
        bytesLeft -= bytesThisChunk;
        if (bytesLeft == 0) {
            break;
        }
        cursorOffset += cursorChunk->length;
        cursorChunk = cursorChunk->next;
        chunkData = cursorChunk->data;
        bytesThisChunk = cursorChunk->length;
    }
    return length;
}

void *Buffer::getRange(uint32_t offset, uint32_t length) {
    // Try the fast path first: is the desired range available in the
    // current chunk?
    uint32_t offsetInChunk;
    Chunk *chunk = cursorChunk;
    if (offset >= cursorOffset) {
        offsetInChunk = offset - cursorOffset;
        if ((offsetInChunk + length) <= chunk->length) {
            return chunk->data + offsetInChunk;
        }
    } else {
        // The cached info is past the desired point; must start
        // searching at the beginning of the buffer.
        chunk = firstChunk;
        offsetInChunk = offset;
    }

    if ((offset >= totalLength) || ((offset + length) > totalLength)) {
        return nullptr;
    }

    // Find the chunk containing the first byte of the range.
    while (offsetInChunk >= chunk->length) {
        offsetInChunk -= chunk->length;
        chunk = chunk->next;
    }
    cursorChunk = chunk;
    cursorOffset = offset - offsetInChunk;

    if ((offsetInChunk + length) <= cursorChunk->length) {
        return cursorChunk->data + offsetInChunk;
    }

    // The desired range is not contiguous. Copy it.
    uint32_t byteAllocated;
    char *data = static_cast<char *>(getNewAllocation(length, &byteAllocated));
    copy(offset, length, data);
    return data;
}

void *Buffer::getNewAllocation(size_t numBytes, uint32_t *bytesAllocated) {
    char *data = new char[numBytes];
    allocations.push_back(data);
    *bytesAllocated = numBytes;
    return data;
}

/**
 * This method implements both the destructor and the reset method.
 * For highest performance, the destructor skips parts of this code.
 * Putting this method here as an in-line allows the compiler to
 * optimize out the parts not needed for the destructor.
 *
 * \param isReset
 *      True means implement the full reset functionality; false means
 *      implement only the functionality need for the destructor.
 */
void Buffer::resetInternal(bool isReset) {
    // Free the chunks.
    Chunk *current = firstChunk;
    while (current != nullptr) {
        Chunk *next = current->next;
        delete current;
        current = next;
    }

    for (auto allocation: allocations) {
        delete[] allocation;
    }

    // Reset state.
    if (isReset) {
        allocations.clear();
        totalLength = 0;
        firstChunk = lastChunk = cursorChunk = nullptr;
        cursorOffset = ~0u;
    }

}


Buffer::Iterator::Iterator(const Buffer *buffer)
    : current(buffer->firstChunk), currentData(), currentLength(), bytesLeft(buffer->totalLength) {
    if (current != nullptr) {
        currentData = current->data;
        currentLength = current->length;
    } else {
        currentData = nullptr;
        currentLength = 0;
    }
}


Buffer::Iterator::Iterator(Buffer *buffer, uint32_t offset, uint32_t length)
    : current(), currentData(), currentLength(), bytesLeft() {
    // Clip offset and length if they are out of range.
    if (offset >= buffer->totalLength) {
        // The iterator's range is empty.
        return;
    }

    // Start from the first chunk of the buffer if the requested range starts
    // before the recently accessed chunk.
    if (offset < buffer->cursorOffset) {
        assert(buffer->firstChunk);
        buffer->cursorChunk = buffer->firstChunk;
        buffer->cursorOffset = 0;
    }
    current = buffer->cursorChunk;
    uint32_t bytesToSkip = offset - buffer->cursorOffset;
    bytesLeft = std::min(length, buffer->totalLength - offset);

    // Advance Iterator up to the first chunk with data from the subrange.
    while (bytesToSkip >= current->length) {
        buffer->cursorOffset += current->length;
        bytesToSkip -= current->length;
        current = current->next;
    }
    buffer->cursorChunk = current;
    currentData = current->data + bytesToSkip;
    currentLength = current->length - bytesToSkip;
    if (bytesLeft < currentLength) {
        currentLength = bytesLeft;
    }
}

Buffer::Iterator::Iterator(const Iterator &other)
    : current(other.current)
      , currentData(other.currentData)
      , currentLength(other.currentLength)
      , bytesLeft(other.bytesLeft) {
}

Buffer::Iterator &Buffer::Iterator::operator=(const Buffer::Iterator &other) {
    if (&other == this)
        return *this;
    current = other.current;
    currentData = other.currentData;
    currentLength = other.currentLength;
    bytesLeft = other.bytesLeft;
    return *this;
}

/**
 * Count the number of distinct chunks of storage covered by the
 * remaining bytes of this iterator.
 */
uint32_t Buffer::Iterator::getNumberChunks() {
    uint32_t bytesLeft = this->bytesLeft;
    if (bytesLeft == 0) {
        return 0;
    }
    if (bytesLeft <= currentLength) {
        return 1;
    }
    bytesLeft -= currentLength;
    Chunk *chunk = current->next;
    uint32_t count = 2;

    while (bytesLeft > chunk->length) {
        bytesLeft -= chunk->length;
        chunk = chunk->next;
        count++;
    }
    return count;
}

/**
 * Advance to the next chunk in the Buffer.
 */
void Buffer::Iterator::next() {
    if (bytesLeft > currentLength) {
        bytesLeft -= currentLength;
        current = current->next;
        currentData = current->data;
        currentLength = std::min(current->length, bytesLeft);
    } else {
        bytesLeft = 0;
        currentData = nullptr;
        currentLength = 0;
    }
}
}
