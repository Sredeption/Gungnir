#ifndef GUNGNIR_LOGCLEANER_H
#define GUNGNIR_LOGCLEANER_H

#include "Context.h"
#include "ConcurrentSkipList.h"

#include <memory>
#include <thread>
#include <list>

namespace Gungnir {

class LogCleaner {

public:
    explicit LogCleaner(Context *context);

    void start();

    void collect(int epoch, ConcurrentSkipList::Node *node);

    void loadEpoch();

    bool clean();

private:

    const static int POLL_USEC = 10000;

    Context *context;

    std::unique_ptr<std::thread> cleaner;

    std::list<std::pair<int, ConcurrentSkipList::Node *>> removals;

    SpinLock lock;

    int minEpoch;

    static void cleanerThread(LogCleaner *logCleaner);
};

}


#endif //GUNGNIR_LOGCLEANER_H
