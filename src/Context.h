#ifndef GUNGNIR_CONTEXT_H
#define GUNGNIR_CONTEXT_H


namespace Gungnir {

class Dispatch;

class WorkerManager;

class Context {
public:
    Dispatch *dispatch;
    WorkerManager *workerManager;

};

}

#endif //GUNGNIR_CONTEXT_H
