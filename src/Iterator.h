#ifndef GUNGNIR_ITERATOR_H
#define GUNGNIR_ITERATOR_H

#include <memory>
#include "Buffer.h"

namespace Gungnir {

class Iterator {
public:
    std::shared_ptr<Buffer> buffer;
    uint32_t size;
    uint32_t offset;

    Iterator();

    explicit Iterator(Buffer *buffer);

    Iterator(const Iterator &that);

    Iterator &operator=(const Iterator &that);

    uint64_t getKey();

    void *getValue(uint32_t &length);

    void next();

    bool isDone();


    ~Iterator();
};

}

#endif //GUNGNIR_ITERATOR_H
