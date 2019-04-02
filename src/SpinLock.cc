
#include <unordered_set>

#include "SpinLock.h"
#include "Cycles.h"
#include "Logger.h"

namespace Gungnir {

/// This namespace is used to keep track of all of the SpinLocks currently
/// in existence, so that we can enumerate them to monitor lock contention.
namespace SpinLockTable {
/**
 * Returns a structure containing the addresses of all SpinLocks
 * currently in existence.
 *
 * There is a function wrapper around this variable to force
 * initialization before usage. This is relevant when SpinLock is
 * initialized in the constructor of a statically declared object.
 */
std::unordered_set<SpinLock *> *allLocks() {
    static std::unordered_set<SpinLock *> map;
    return &map;
}

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
SpinLock::SpinLock(std::string name)
    : mutex(false)
      , name(std::move(name))
      , acquisitions(0)
      , contendedAcquisitions(0)
      , contendedTicks(0)
      , logWaits(false) {
    std::lock_guard<std::mutex> lock(*SpinLockTable::lock());
    SpinLockTable::allLocks()->insert(this);
}

SpinLock::~SpinLock() {
    std::lock_guard<std::mutex> lock(*SpinLockTable::lock());
    SpinLockTable::allLocks()->erase(this);
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
                            "%s SpinLock locked for one second; deadlock?",
                            name.c_str());
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

/**
 * Change the name of the SpinLock. The name is intended to give some hint as
 * to the purpose of the lock, where it was declared, etc.
 *
 * \param name
 *      The string name to give this lock.
 */
void
SpinLock::setName(std::string name) {
    this->name = std::move(name);
}

/**
 * Return the total of SpinLocks currently in existence; intended
 * primarily for testing.
 */
int
SpinLock::numLocks() {
    std::lock_guard<std::mutex> lock(*SpinLockTable::lock());
    return static_cast<int>(SpinLockTable::allLocks()->size());
}
}
