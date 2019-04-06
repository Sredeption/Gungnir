#ifndef GUNGNIR_CONTEXT_H
#define GUNGNIR_CONTEXT_H


namespace Gungnir {

class Dispatch;

class WorkerManager;

class Transport;

class ConcurrentSkipList;

class LogCleaner;

class OptionConfig;

class Context {
public:
    Dispatch *dispatch;
    WorkerManager *workerManager;
    Transport *transport;
    ConcurrentSkipList *skipList;
    LogCleaner *logCleaner;
    OptionConfig *optionConfig;

    explicit Context(OptionConfig &optionConfig, bool hasDedicatedDispatchThread);

    ~Context();
};

}

#endif //GUNGNIR_CONTEXT_H
