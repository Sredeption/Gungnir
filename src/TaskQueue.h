#ifndef GUNGNIR_TASKQUEUE_H
#define GUNGNIR_TASKQUEUE_H

#include <queue>
#include "Context.h"

namespace Gungnir {

class TaskQueue;

class Task {
public:
    explicit Task();

    virtual ~Task();


    virtual void performTask() = 0;

    bool isScheduled();

protected:
    void schedule();

    TaskQueue *taskQueue;

private:
    bool scheduled;

    friend class TaskQueue;
};

class TaskQueue {
public:

    explicit TaskQueue(Context *context);

    virtual ~TaskQueue() = default;

    bool isIdle();

    virtual bool performTask() = 0;

    void schedule(Task *task);

protected:
    Context *context;

    std::queue<Task *> tasks;

    Task *getNextTask();
};

}


#endif //GUNGNIR_TASKQUEUE_H
