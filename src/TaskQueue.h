#ifndef GUNGNIR_TASKQUEUE_H
#define GUNGNIR_TASKQUEUE_H

#include <queue>
#include "Context.h"

namespace Gungnir {

class TaskQueue;

class Task {
    explicit Task(TaskQueue &taskQueue);

    virtual ~Task();


    virtual void performTask() = 0;

    bool isScheduled();

    void schedule();

protected:
    /// Executes this Task when it isScheduled() on taskQueue.performTask().
    TaskQueue &taskQueue;

private:
    /// True if performTask() will be run on the next taskQueue.performTask().
    bool scheduled;

    friend class TaskQueue;
};

class TaskQueue {
public:

    TaskQueue(Context *context);

    ~TaskQueue();

    bool performTask();

    void schedule(Task *task);

private:
    std::queue<Task *> tasks;
};

}


#endif //GUNGNIR_TASKQUEUE_H
