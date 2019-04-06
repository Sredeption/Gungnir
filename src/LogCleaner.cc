#include "LogCleaner.h"
#include "Common.h"
#include "WorkerManager.h"
#include "Context.h"

namespace Gungnir {

LogCleaner::LogCleaner(Context *context) :
    context(context), cleaner(), lock("LogCleaner"), minEpoch(-1) {

}

void LogCleaner::start() {
    cleaner = std::make_unique<std::thread>(cleanerThread, this);
}

void LogCleaner::collect(int epoch, ConcurrentSkipList::Node *node) {
    SpinLock::Guard guard(lock);
    removals.emplace_back(epoch, node);
}

void LogCleaner::loadEpoch() {
    context->workerManager->minEpoch.load();
}

bool LogCleaner::clean() {
    bool workDone = false;
    ConcurrentSkipList::Node *node = nullptr;
    {
        SpinLock::Guard guard(lock);
        auto removal = removals.front();
        if (removal.first < minEpoch) {
            node = removal.second;
            removals.pop_front();
            workDone = true;
        }
    }
    delete node;

    return workDone;
}

void LogCleaner::cleanerThread(LogCleaner *logCleaner) {
    while (true) {
        while (logCleaner->clean());
        logCleaner->loadEpoch();
        if (!logCleaner->clean()) {
            useconds_t r = static_cast<useconds_t>(generateRandom() % POLL_USEC) / 10;
            usleep(r);
        }
    }
}

}
