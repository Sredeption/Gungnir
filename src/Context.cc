#include "Context.h"
#include "Dispatch.h"
#include "WorkerManager.h"
#include "TcpTransport.h"

namespace Gungnir {

Context::Context(OptionConfig &optionConfig, bool hasDedicatedDispatchThread) :
    dispatch(nullptr), workerManager(nullptr), transport(nullptr) {
    dispatch = new Dispatch(hasDedicatedDispatchThread);
    workerManager = new WorkerManager();
    transport = new TcpTransport(this, optionConfig.serverLocator);
}

Context::~Context() {

}
}
