#include "Server.h"
#include "Dispatch.h"
#include "WorkerManager.h"
#include "ConcurrentSkipList.h"
#include "OptionConfig.h"
#include "LogCleaner.h"

namespace Gungnir {

Server::Server(Context *context) :
    context(context) {
    context->skipList = new ConcurrentSkipList(context);
    context->workerManager = new WorkerManager(context, context->optionConfig->maxCores);
    context->logCleaner = new LogCleaner(context);
}

Server::~Server() {
    delete context->skipList;
    delete context->workerManager;
}

void Server::run() {

    Dispatch &dispatch = *context->dispatch;

    context->logCleaner->start();

    dispatch.run();
}
}
