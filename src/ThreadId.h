#ifndef GUNGNIR_THREADID_H
#define GUNGNIR_THREADID_H

#include <mutex>

namespace Gungnir {


class ThreadId {
public:
    static int get();

private:
    explicit ThreadId() = default;

    static int assign();

    static __thread int id;
    static int highestId;
    static std::mutex mutex;
};

}

#endif //GUNGNIR_THREADID_H
