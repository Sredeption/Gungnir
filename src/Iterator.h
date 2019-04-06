#ifndef GUNGNIR_ITERATOR_H
#define GUNGNIR_ITERATOR_H

#include "Buffer.h"

namespace Gungnir {

class Iterator {
public:
    Buffer *buffer;
    uint32_t size;
    uint32_t offset;

    Iterator();

    explicit Iterator(Buffer *buffer);

    uint64_t getKey();

    void *getValue(uint32_t &length);

    void next();

    bool isDone();


    ~Iterator();
};

}

#endif //GUNGNIR_ITERATOR_H
