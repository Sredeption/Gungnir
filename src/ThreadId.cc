#include "ThreadId.h"

namespace Gungnir {
// Thread-specific data holds the identifier for each thread.  It starts
// off zero, but is set to a non-zero unique value the first time it is
// accessed.
__thread int ThreadId::id = 0;

// The highest-numbered thread identifier that has already been used.
int ThreadId::highestId = 0;

/// Used to serialize access to #highestId.
std::mutex ThreadId::mutex;

/**
 * Pick a unique value to use as the thread identifier for the current
 * thread. This value is saved in the thread-specific variable #Thread::id.
 *
 * \return
 *      The unique identifier for the current thread.
 */
int
ThreadId::assign() {
    std::unique_lock<std::mutex> lock(mutex);
    if (id == 0) {
        highestId++;
        id = highestId;
    }
    return id;
}

/**
 * Return a unique identifier associated with this thread.  The
 * return value has two properties:
 * - It will never be zero.
 * - It will be unique for this thread (i.e., no other thread has ever
 *   been returned this value or ever will be returned this value)
 */
int
ThreadId::get() {
    if (id != 0) {
        return id;
    }
    return assign();
}
}
