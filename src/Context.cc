#include "Context.h"
#include "Dispatch.h"
#include "TcpTransport.h"
#include "ConcurrentSkipList.h"
#include "OptionConfig.h"

namespace Gungnir {

Context::Context(OptionConfig &optionConfig, bool hasDedicatedDispatchThread) :
    dispatch(nullptr), workerManager(nullptr), transport(nullptr), skipList(nullptr), optionConfig(&optionConfig) {
    dispatch = new Dispatch(hasDedicatedDispatchThread);
    transport = new TcpTransport(this, optionConfig.serverLocator);
}

Context::~Context() {
    delete dispatch;
    delete transport;
    delete skipList;
}
}
