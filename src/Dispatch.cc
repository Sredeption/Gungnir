#include <cassert>
#include <cstring>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <cstdio>
#include <unistd.h>

#include "Dispatch.h"
#include "ThreadId.h"
#include "Logger.h"
#include "Exception.h"

namespace Gungnir {
Dispatch::Dispatch(bool hasDedicatedThread)
    : ownerId(ThreadId::get()), hasDedicatedThread(hasDedicatedThread), pollers(), files(), epollFd(-1), epollThread()
      , exitPipeFds(), readyFd(-1), readyEvents(0u), fileInvocationSerial(0) {


}

Dispatch::~Dispatch() {
    if (epollThread) {
        // Writing data to the pipe below signals the epoll thread that it
        // should exit.
        write(exitPipeFds[1], "x", 1);
        epollThread->join();
        epollThread.release();
        close(exitPipeFds[0]);
        close(exitPipeFds[1]);
        exitPipeFds[1] = exitPipeFds[0] = -1;
    }
    if (epollFd >= 0) {
        close(epollFd);
        epollFd = -1;
    }
    for (auto &poller : pollers) {
        poller->owner = nullptr;
        poller->slot = -1;
    }
    pollers.clear();
    for (auto &file : files) {
        if (file != nullptr) {
            file->owner = nullptr;
            file->active = false;
            file->events = 0;
            file = nullptr;
        }
    }
    readyFd.store(-1, std::memory_order_release);
}

bool Dispatch::isDispatchThread() {
    return (!hasDedicatedThread || ownerId == ThreadId::get());
}

/**
 * Check to see if any events need handling.
 *
 * \return
 *      The return value is a count of the number of useful actions
 *      taken during this call (the number of pollers, timer handlers,
 *      and file handlers that did useful work). Zero means there
 *      was no useful work found in this call.
 */
int Dispatch::poll() {
    assert(isDispatchThread());
    int result = 0;

    for (auto &poller : pollers) {
        result += poller->poll();
    }
    if (readyFd >= 0) {
        int fd = readyFd;

        // Make sure that the read of readyEvents doesn't get reordered either
        // before we see readyFd or after we change it (otherwise could
        // read the wrong value).
        uint32_t events = readyEvents.load(std::memory_order_acquire);
        readyFd.store(-1, std::memory_order_release);
        File *file = files[fd];
        if (file) {
            int id = fileInvocationSerial + 1;
            if (id == 0) {
                id++;
            }
            fileInvocationSerial = id;
            file->invocationId = id;

            // It's possible that the desired events may have changed while
            // an event was being reported.
            // events &= file->events;
            if (events != 0) {
                file->handleFileEvent(events);
                result++;
            }

            // Must reenable the event for this file, since it was automatically
            // disabled by epoll.  However, it's possible that the handler
            // deleted the File; don't do anything if that appears to have
            // happened.  By using a unique invocation id instead of a simple
            // boolean we can detect if the old handler was deleted and a new
            // handler was created for the same fd.
            if ((files[fd] == file) && (file->invocationId == id)) {
                file->invocationId = 0;
                file->setEvents(file->events);
            }
        }
    }

    return result;
}

Dispatch::Poller::Poller(Dispatch *dispatch, std::string pollerName)
    : owner(dispatch), pollerName(std::move(pollerName)), slot(static_cast<int>(owner->pollers.size())) {

    owner->pollers.push_back(this);
}

Dispatch::Poller::~Poller() {
    if (slot < 0) {
        return;
    }

    // Erase this Poller from the vector by overwriting it with the
    // poller that used to be the last one in the vector.
    //
    // Note: this approach is reentrant (it is safe to delete a
    // poller from a poller callback, which means that the poll
    // method is in the middle of scanning the list of all pollers;
    // the worst that will happen is that the poller that got moved
    // may not be invoked in the current scan).
    owner->pollers[slot] = owner->pollers.back();
    owner->pollers[slot]->slot = slot;
    owner->pollers.pop_back();
    slot = -1;
}

Dispatch::File::File(Dispatch *dispatch, int fd, int events)
    : owner(dispatch), fd(fd), events(0), active(false), invocationId(0) {
    // Start the polling thread if it doesn't already exist (and also create
    // the epoll file descriptor and the exit pipe).
    if (!owner->epollThread) {
        owner->epollFd = epoll_create(10);
        if (owner->epollFd < 0) {
            throw FatalError(HERE, "epoll_create failed in Dispatch", errno);
        }
        if (pipe(owner->exitPipeFds) != 0) {
            throw FatalError(HERE,
                             "Dispatch couldn't create exit pipe for epoll thread",
                             errno);
        }
        epoll_event epollEvent{};
        // The following statement is not needed, but without it valgrind
        // will generate false errors about uninitialized data.
        epollEvent.data.u64 = 0;
        epollEvent.events = EPOLLIN | EPOLLONESHOT;

        // -1 fd signals to epoll thread to exit.
        epollEvent.data.fd = -1;
        if (epoll_ctl(owner->epollFd, EPOLL_CTL_ADD,
                      owner->exitPipeFds[0], &epollEvent) != 0) {
            throw FatalError(HERE,
                             "Dispatch couldn't set epoll event for exit pipe",
                             errno);
        }
        owner->epollThread = std::make_unique<std::thread>(Dispatch::epollThreadMain, owner);
    }

    if (owner->files.size() <= static_cast<uint32_t>(fd)) {
        owner->files.resize(2 * fd);
    }
    if (owner->files[fd] != nullptr) {
        throw FatalError(HERE, "can't have more than 1 Dispatch::File "
                               "for a file descriptor");
    }
    owner->files[fd] = this;
    if (events != 0) {
        setEvents(events);
    }
}

/**
 * This function is invoked in a separate thread; its job is to invoke
 * epoll and report back whenever epoll returns information about an
 * event.  By putting this functionality in a separate thread the main
 * poll loop never needs to incur the overhead of a kernel call.
 *
 * \param owner
 *      The dispatch object on whose behalf this thread is working.
 */
void Dispatch::epollThreadMain(Dispatch *owner) try {
#define MAX_EVENTS 10
    struct epoll_event events[MAX_EVENTS];
    while (true) {
        int count = epoll_wait(owner->epollFd, events, MAX_EVENTS, -1);
        if (count <= 0) {
            if (count == 0) {
                Logger::log(HERE, "epoll_wait returned no events in "
                                  "Dispatch::epollThread");
                continue;
            } else {
                if (errno == EINTR)
                    continue;
                Logger::log(HERE, "epoll_wait failed in Dispatch::epollThread: %s",
                            strerror(errno));
                return;
            }
        }

        // Signal all of the ready file descriptors back to the main
        // polling loop using a shared memory location.
        for (int i = 0; i < count; i++) {
            int fd = events[i].data.fd;
            uint32_t readyEvents = 0;
            if (events[i].events & EPOLLIN) {
                readyEvents |= READABLE;
            }
            if (events[i].events & EPOLLOUT) {
                readyEvents |= WRITABLE;
            }
            if (fd == -1) {
                // This is a special value associated with exitPipeFd[0],
                // and indicates that this thread should exit.
                return;
            }
            while (owner->readyFd.load(std::memory_order_acquire) >= 0) {
                // The main polling loop hasn't yet noticed the last file that
                // became ready; wait for the shared memory location to clear
                // again. It's also possible the main thread has signaled for
                // this thread to exit and isn't interested in this readyFd
                // value, so check on that while waiting.
                if (owner->exitPipeFds[0] >= 0 &&
                    fdIsReady(owner->exitPipeFds[0])) {
                    return;
                }
            }
            owner->readyEvents = readyEvents;
            // The following line guarantees that the modification of
            // owner->readyEvents will be visible in memory before the
            // modification of readyFd.
            owner->readyFd.store(events[i].data.fd, std::memory_order_release);
        }
    }
} catch (const std::exception &e) {
    Logger::log(HERE, "Fatal error in epollThreadMain: %s", e.what());
    throw;
} catch (...) {
    Logger::log(HERE, "Unknown fatal error in epollThreadMain.");
    throw;
}

bool Dispatch::fdIsReady(int fd) {
    assert(fd >= 0);
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval timeout{0, 0};
    int r = select(fd + 1, &fds, &fds, &fds, &timeout);
    if (r < 0) {
        throw FatalError(HERE,
                         "select error on Dispatch's exitPipe", errno);
    }
    return r > 0;
}

}
