#include "TaskQueue.h"

namespace Gungnir {

Task::Task() {

}

Task::~Task() {

}

void Task::schedule() {
    taskQueue->schedule(this);
}

bool Task::isScheduled() {
    return scheduled;
}

TaskQueue::TaskQueue(Context *context) : context(context), tasks() {

}

void TaskQueue::schedule(Task *task) {
    task->taskQueue = this;
    task->scheduled = true;
    tasks.push(task);
}

bool TaskQueue::isIdle() {
    return tasks.empty();
}

Task *TaskQueue::getNextTask() {
    if (tasks.empty())
        return nullptr;
    Task *task = tasks.front();
    tasks.pop();
    task->scheduled = false;
    return task;
}

}
