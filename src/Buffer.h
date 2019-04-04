#ifndef GUNGNIR_BUFFER_H
#define GUNGNIR_BUFFER_H

#include <cstdio>
#include <cstdint>
#include <vector>

namespace Gungnir {

class Buffer {
public:
    class Chunk {
    public:
        Chunk(void *data, uint32_t length);

        virtual ~Chunk();

        /// Next Chunk in Buffer, or NULL if this is the last Chunk.
        Chunk *next;

        /// First byte of valid data in this Chunk.
        char *data;

        /// The number of valid bytes currently stored in the Chunk.
        uint32_t length;

        Chunk(const Chunk &) = delete;

        Chunk &operator=(const Chunk &) = delete;
    };

    Buffer();

    ~Buffer();

    Buffer(const Buffer &) = delete;

    Buffer &operator=(const Buffer &) = delete;

    void *alloc(size_t numBytes);

    void append(const void *data, uint32_t numBytes);

    void append(Buffer *src, uint32_t offset = 0, uint32_t length = ~0);

    inline uint32_t
    size() const {
        return totalLength;
    }

    uint32_t peek(uint32_t offset, void **returnPtr);

    virtual void reset();

    void truncate(uint32_t newLength);

    void truncateFront(uint32_t bytesToDelete);

    uint32_t write(uint32_t offset, uint32_t length, FILE *f);

    uint32_t getNumberChunks();

    class Iterator {
    public:
        explicit Iterator(const Buffer *buffer);

        Iterator(Buffer *buffer, uint32_t offset, uint32_t length);

        Iterator(const Iterator &other);

        ~Iterator() = default;

        Iterator &operator=(const Iterator &other);

        /**
         * Return a pointer to the first byte of the data that is available
         * contiguously at the current iterator position, or NULL if the
         * iteration is complete.
         */
        const void *
        getData() const {
            return currentData;
        }

        /**
         * Return the number of bytes of contiguous data available at the
         * current position (and part of the iterator's range), or zero if
         * the iteration is complete.
         */
        inline uint32_t
        getLength() const {
            return currentLength;
        }

        uint32_t getNumberChunks();

        /**
         * Indicate whether the entire range has been iterated.
         * \return
         *      True means there are no more bytes left in the range, and
         *      #next(), #getData(), and #getLength should not be called again.
         *      False means there are more bytes left.
         */
        inline bool
        isDone() const {
            return currentLength == 0;
        }

        void next();

        /**
         * Returns the total number bytes left to iterate, including those
         * in the current chunk.
         */
        uint32_t size() { return bytesLeft; }

    private:
        /// The current chunk over which we're iterating.  May be NULL.
        Chunk *current;

        /// The first byte of data available at the current iterator
        /// position (NULL means no more bytes are available).
        char *currentData;

        /// The number of bytes of data available at the current
        /// iterator position. 0 means that iteration has finished.
        uint32_t currentLength;

        /// The number of bytes left to iterate, including those at the
        /// current iterator position.
        uint32_t bytesLeft;

        friend class Buffer;
    };

private:
    uint32_t totalLength;

    Chunk *firstChunk;
    Chunk *lastChunk;

    Chunk *cursorChunk;
    uint32_t cursorOffset;

    std::vector<char *> allocations;

    void *getNewAllocation(size_t numBytes);

    void resetInternal(bool isReset);
};



}

#endif //GUNGNIR_BUFFER_H
