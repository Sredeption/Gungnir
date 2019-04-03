//
// Created by issac on 4/3/19.
//

#ifndef GUNGNIR_OBJECTPOOL_H
#define GUNGNIR_OBJECTPOOL_H

#include <memory>

namespace Gungnir {


template<typename T>
class ObjectPool {
public:
    ObjectPool() : alloc() {

    }

    ~ObjectPool() = default;

    template<typename... Args>
    T *
    construct(Args &&... args) {

        T *object = alloc.allocate(1);
        alloc.construct(object, args...);

        return object;
    }

    void
    destroy(T *object) {
        alloc.destroy(object);
        alloc.deallocate(object, 1);
    }

private:
    std::allocator<T> alloc;

};
}


#endif //GUNGNIR_OBJECTPOOL_H
