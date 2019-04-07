
#include <unordered_set>

#include "SpinLock.h"
#include "Cycles.h"
#include "Logger.h"

namespace Gungnir {

/// This namespace is used to keep track of all of the SpinLocks currently
/// in existence, so that we can enumerate them to monitor lock contention.
namespace SpinLockTable {
/**
 * This mutex protects the map pointed to by "allLocks()".
 *
 * See the comment above for why this is a function and not a variable.
 */
std::mutex *lock() {
    static std::mutex mutex;
    return &mutex;
}
} // namespace SpinLockTable

/**
 * Construct a new SpinLock and give it the provided name.
 */
SpinLock::SpinLock()
    : mutex(false)
      , acquisitions(0)
      , contendedAcquisitions(0)
      , contendedTicks(0)
      , logWaits(false) {
}

SpinLock::~SpinLock() {
    std::lock_guard<std::mutex> lock(*SpinLockTable::lock());
}

/**
 * Acquire the SpinLock; blocks the thread (by continuously polling the lock)
 * until the lock has been acquired.
 */
void
SpinLock::lock() {
    uint64_t startOfContention = 0;

    while (mutex.test_and_set(std::memory_order_acquire)) {
        if (startOfContention == 0) {
            startOfContention = Cycles::rdtsc();
        } else {
            uint64_t now = Cycles::rdtsc();
            if (Cycles::toSeconds(now - startOfContention) > 1.0) {
                Logger::log(HERE,
                            "SpinLock locked for one second; deadlock?");
                contendedTicks += now - startOfContention;
                startOfContention = now;
            }
        }
    }

    if (startOfContention != 0) {
        contendedTicks += (Cycles::rdtsc() - startOfContention);
        contendedAcquisitions++;
    }
    acquisitions++;
}

/**
 * Try to acquire the SpinLock; does not block the thread and returns
 * immediately.
 *
 * \return
 *      True if the lock was successfully acquired, false if it was already
 *      owned by some other thread.
 */
bool
SpinLock::try_lock() {
    // test_and_set sets the flag to true and returns the previous value;
    // if it's True, someone else is owning the lock.
    return !mutex.test_and_set(std::memory_order_acquire);
}

/**
 * Release the SpinLock.  The caller must previously have acquired the
 * lock with a call to #lock or #try_lock.
 */
void
SpinLock::unlock() {
    mutex.clear(std::memory_order_release);
}

}
