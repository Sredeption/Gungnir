#ifndef GUNGNIR_CONTEXT_H
#define GUNGNIR_CONTEXT_H

#include "OptionConfig.h"

namespace Gungnir {

class Dispatch;

class WorkerManager;

class Transport;

class Context {
public:
    Dispatch *dispatch;
    WorkerManager *workerManager;
    Transport *transport;

    explicit Context(OptionConfig &optionConfig, bool hasDedicatedDispatchThread);

    ~Context();
};

}

#endif //GUNGNIR_CONTEXT_H
