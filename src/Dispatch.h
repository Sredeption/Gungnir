#ifndef GUNGNIR_DISPATCH_H
#define GUNGNIR_DISPATCH_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "Exception.h"

namespace Gungnir {

class Dispatch {

public:
    explicit Dispatch(bool hasDedicatedThread);

    ~Dispatch();

    bool isDispatchThread();

    int poll();

    /**
     * A Poller object is invoked once each time through the dispatcher's
     * inner polling loop.
     */
    class Poller {
    public:
        explicit Poller(Dispatch *dispatch, std::string pollerName);

        virtual ~Poller();

        /**
         * This method is defined by a subclass and invoked once by the
         * dispatcher during each pass through its inner polling loop.
         *
         * \return
         *      1 means that this poller did useful work during this call.
         *      0 means that the poller found no work to do.
         */
        virtual int poll() = 0;

        Poller(const Poller &) = delete;

        Poller &operator=(const Poller &) = delete;

    protected:
        /// The Dispatch object that owns this Poller.  NULL means the
        /// Dispatch has been deleted.
        Dispatch *owner;

        /// Human-readable string name given to the poller to make it
        /// easy to identify for debugging. For most pollers just passing
        /// in the subclass name probably makes sense.
        std::string pollerName;

    private:
        /// Index of this Poller in Dispatch::pollers.  Allows deletion
        /// without having to scan all the entries in pollers. -1 means
        /// this poller isn't currently in Dispatch::pollers (happens
        /// after Dispatch::reset).
        int slot;

        friend class Dispatch;

    };

    /**
     * Defines the kinds of events for which File handlers can be defined
     * (some combination of readable and writable).
     */
    enum FileEvent {
        READABLE = 1,
        WRITABLE = 2
    };

    /**
     * A File object is invoked whenever its associated fd is readable
     * and/or writable.
     */
    class File {
    public:
        explicit File(Dispatch *dispatch, int fd, int events = 0);

        virtual ~File();

        void setEvents(uint32_t events);

        /**
         * This method is defined by a subclass and invoked by the dispatcher
         * whenever an event associated with the object has occurred. If
         * the event still exists when this method returns (e.g., the file
         * is readable but the method did not read the data), then the method
         * will be invoked again. During the execution of this method events
         * for this object are disabled (calling Dispatch::poll will not cause
         * this method to be invoked).
         *
         * \param events
         *      Indicates whether the file is readable or writable or both
         *      (OR'ed combination of FileEvent values).
         */
        virtual void handleFileEvent(uint32_t events) = 0;

        File(const File &) = delete;

        File &operator=(const File &) = delete;

    protected:
        /// The Dispatch object that owns this File.  NULL means the
        /// Dispatch has been deleted.
        Dispatch *owner;

        /// The file descriptor passed into the constructor.
        int fd;

        /// The events that are currently being watched for this file.
        int events;

        /// Indicates whether epoll_ctl(EPOLL_CTL_ADD) has been called.
        bool active;

        /// Non-zero means that handleFileEvent has been invoked but hasn't
        /// yet returned; the actual value is a (reasonably) unique identifier
        /// for this particular invocation.  Zero means handleFileEvent is not
        /// currently active.  This field is used to defer the effect of
        /// setEvents until after handleFileEvent returns.
        int invocationId;

    private:

        friend class Dispatch;


    };

private:

    static void epollThreadMain(Dispatch *owner);

    static bool fdIsReady(int fd);

    int ownerId;

    bool hasDedicatedThread;
    // Keeps track of all of the pollers currently defined.  We don't
    // use an intrusive list here because it isn't reentrant: we need
    // to add/remove elements while the dispatcher is traversing the list.
    std::vector<Poller *> pollers;

    // Keeps track of of all of the file handlers currently defined.
    // Maps from a file descriptor to the corresponding File, or NULL
    // if none.
    std::vector<File *> files;

    // The file descriptor used for epoll.
    int epollFd;

    // We start a separate thread to invoke epoll kernel calls, so the
    // main polling loop is not delayed by kernel calls.  This thread
    // is only used when there are active Files.
    std::unique_ptr<std::thread> epollThread;

    // Read and write descriptors for a pipe.  The epoll thread always has
    // the read fd for this pipe in its active set; writing data to the pipe
    // causes the epoll thread to exit.  These file descriptors are only
    // valid if #epollThread is non-null.
    int exitPipeFds[2];

    // Used for communication between the epoll thread and #poll: the
    // epoll thread stores a fd here when it becomes ready, and #poll
    // resets this back to -1 once it has retrieved the fd.
    std::atomic<int> readyFd;

    // Also used for communication between epoll thread and #poll:
    // before setting readyFd the epoll thread stores information here
    // about which events fired for readyFd (OR'ed combination of
    // FileEvent values).
    std::atomic<uint32_t> readyEvents;

    // Used to assign a (nearly) unique identifier to each invocation
    // of a File.
    int fileInvocationSerial;

};

Dispatch::File::~File() {
    if (owner == nullptr) {
        // Dispatch object has already been deleted; don't do anything.
        return;
    }

    if (active) {
        // Note: don't worry about errors here. For example, it's
        // possible that the file was closed before this destructor
        // was invoked, in which case EBADF will occur.
        epoll_ctl(owner->epollFd, EPOLL_CTL_DEL, fd, nullptr);
    }
    owner->files[fd] = nullptr;
}

void Dispatch::File::setEvents(uint32_t events) {
    if (owner == nullptr) {
        // Dispatch object has already been deleted; don't do anything.
        return;
    }

    epoll_event epollEvent{};
    // The following statement is not needed, but without it valgrind
    // will generate false errors about uninitialized data.
    epollEvent.data.u64 = 0;
    this->events = events;
    if (invocationId != 0) {
        // Don't communicate anything to epoll while a call to
        // operator() is in progress (don't want another instance of
        // the handler to be invoked until the first completes): we
        // will get another chance to update epoll state when the handler
        // completes.
        return;
    }
    epollEvent.events = 0;
    if (events & READABLE) {
        epollEvent.events |= EPOLLIN | EPOLLONESHOT;
    }
    if (events & WRITABLE) {
        epollEvent.events |= EPOLLOUT | EPOLLONESHOT;
    }
    epollEvent.data.fd = fd;
    if (epoll_ctl(owner->epollFd,
                  active ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, fd, &epollEvent) != 0) {
        throw FatalError(HERE, format("Dispatch couldn't set epoll event "
                                      "for fd %d", fd), errno);
    }
    active = true;
}


}

#endif //GUNGNIR_DISPATCH_H
