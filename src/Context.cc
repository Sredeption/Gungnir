#include "Context.h"
#include "Dispatch.h"
#include "TcpTransport.h"
#include "ConcurrentSkipList.h"
#include "OptionConfig.h"

namespace Gungnir {

Context::Context() :
    dispatch(nullptr), workerManager(nullptr), transport(nullptr), skipList(nullptr), logCleaner(nullptr)
    , optionConfig(nullptr) {

}

Context::Context(OptionConfig &optionConfig, bool hasDedicatedDispatchThread) :
    dispatch(nullptr), workerManager(nullptr), transport(nullptr), skipList(nullptr), logCleaner(nullptr)
    , optionConfig(&optionConfig) {
    dispatch = new Dispatch(hasDedicatedDispatchThread);
    transport = new TcpTransport(this, optionConfig.serverLocator);
}

Context::~Context() {
    delete dispatch;
    delete transport;
    delete skipList;
}
}
