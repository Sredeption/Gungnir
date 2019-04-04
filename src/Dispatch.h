#ifndef GUNGNIR_DISPATCH_H
#define GUNGNIR_DISPATCH_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "Exception.h"
#include "SpinLock.h"

namespace Gungnir {

class Dispatch {

public:
    explicit Dispatch(bool hasDedicatedThread);

    ~Dispatch();

    bool isDispatchThread();

    int poll();

    void run() __attribute__ ((noreturn));

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

    /**
     * Lock objects are used to synchronize between the dispatch thread and
     * other threads.  As long as a Lock object exists the following guarantees
     * are in effect: either (a) the thread is the dispatch thread or (b) no
     * other non-dispatch thread has a Lock object and the dispatch thread is
     * in an idle state waiting for the Lock to be destroyed.  Although Locks
     * are intended primarily for use in non-dispatch threads, they can also be
     * used in the dispatch thread (e.g., if you can't tell which thread will
     * run a particular piece of code). Locks may not be used recursively: a
     * single thread can only create a single Lock object at a time.
     */
    class Lock {
    public:
        explicit Lock(Dispatch *dispatch);

        ~Lock();

    private:
        /// The Dispatch object associated with this Lock.
        Dispatch *dispatch;

        /// Used to lock Dispatch::mutex, but only if the Lock object
        /// is constructed in a thread other than the dispatch thread
        /// (no mutual exclusion is needed if the Lock is created in
        /// the dispatch thread).
        std::unique_ptr<std::lock_guard<SpinLock>> lock;
    };

private:

    static void epollThreadMain(Dispatch *owner);

    static bool fdIsReady(int fd);

    int ownerId;
    // Used to make sure that only one thread at a time attempts to lock
    // the dispatcher.
    SpinLock mutex;

    // Nonzero means there is a (non-dispatch) thread trying to lock the
    // dispatcher.
    std::atomic<int> lockNeeded;

    // Nonzero means the dispatch thread is locked.
    std::atomic<int> locked;

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

}


#endif //GUNGNIR_DISPATCH_H
