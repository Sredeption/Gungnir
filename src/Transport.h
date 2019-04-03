#include <utility>

#include <utility>
#include <atomic>
#include <cassert>

#ifndef GUNGNIR_TRANSPORT_H
#define GUNGNIR_TRANSPORT_H


#include "CodeLocation.h"
#include "Exception.h"
#include "Buffer.h"

namespace Gungnir {
/**
 * An exception that is thrown when the Transport class encounters a problem.
 */
struct TransportException : public Exception {
    explicit TransportException(const CodeLocation &where)
        : Exception(where) {}

    TransportException(const CodeLocation &where, std::string msg)
        : Exception(where, std::move(msg)) {}

    TransportException(const CodeLocation &where, int errNo)
        : Exception(where, errNo) {}

    TransportException(const CodeLocation &where, const std::string &msg, int errNo)
        : Exception(where, msg, errNo) {}
};

/**
 * An interface for reliable communication across the network.
 *
 * Implementations all send and receive RPC messages reliably over the network.
 * These messages are variable-length and can be larger than a single network
 * frame in size.
 *
 * Implementations differ in the protocol stacks they use and their performance
 * characteristics.
 */
class Transport {
public:
    class RpcNotifier {
    public:
        explicit RpcNotifier() = default;

        virtual ~RpcNotifier() = default;

        virtual void completed();

        virtual void failed();

    };

    static const uint32_t MAX_RPC_LEN = ((1 << 23) + 200);

    /**
     * An RPC request that has been received and is either being serviced or
     * waiting for service.
     */
    class ServerRpc {
    protected:
        /**
         * Constructor for ServerRpc.
         */
        ServerRpc()
            : requestPayload()
              , replyPayload()
              , epoch(0)
              , activities(~0) {}

    public:
        /**
         * Destructor for ServerRpc.
         */
        virtual ~ServerRpc() {}

        /**
         * Respond to the RPC with the contents of #replyPayload.
         *
         * You should discard all pointers to this #ServerRpc object after this
         * call.
         *
         * \throw TransportException
         *      If the client has crashed.
         */
        virtual void sendReply() = 0;

        /**
         * Return a ServiceLocator string describing the client that initiated
         * this RPC. Since it's a client this string can't be used to address
         * anything, but it can be useful for debugging purposes.
         *
         * Note that for performance reasons Transports may defer constructing
         * the string until this method is called (since it is expected to be
         * used only off of the fast path).
         */
        virtual std::string getClientServiceLocator() = 0;

        /**
         * Returns false if the epoch was not set, else true. Used to assert
         * that no RPCs are pushed through the WorkerManager without an epoch.
         */
        bool
        epochIsSet() {
            return epoch != 0;
        }

        /**
         * The incoming RPC payload, which contains a request.
         */
        Buffer requestPayload;

        /**
         * The RPC payload to send as a response with #sendReply().
         */
        Buffer replyPayload;

        /**
         * The epoch of this RPC upon reception. ServerRpcPool will set this
         * value automatically, tagging the incoming RPC before it is passed to
         * the handling service's dispatch method. This number can be used later
         * to determine if any RPCs less than or equal to a certain epoch are
         * still outstanding in the system.
         */
        uint64_t epoch;

        /**
         * A bit mask indicating what sorts of actions are being performed
         * during this RPC (default: ~0, which means all activities).
         * Individual RPCs can replace the default with a more selective
         * value so that the RPCs will be ignored in some cases when
         * scanning epochs. 0 means the RPC isn't doing anything that
         * matters to anyone.
         */
        int activities;

        /**
         * Bit values for activities above.
         * READ_ACTIVITY:             RPC is reading log information
         * APPEND_ACTIVITY:           RPC may add new entries to the log
         */
        static const int READ_ACTIVITY = 1;
        static const int APPEND_ACTIVITY = 2;

    };

    /**
     * A handle to send RPCs to a particular service.
     */
    class Session {
    public:
        explicit Session(std::string serviceLocator)
            : refCount(0), serviceLocator(std::move(serviceLocator)) {}

        virtual ~Session() {
            assert(refCount == 0);
        }

        /**
         * This method is invoked via the boost intrusive_ptr mechanism when
         * all copies of the SessionRef for this session have been deleted; it
         * should reclaim the storage for the session.  This method is invoked
         * (rather than just deleting the object) to enable transport-specific
         * memory allocation for sessions.  The default is to delete the
         * Session object.
         */
        virtual void release() {
            delete this;
        }

        /**
         * Initiate the transmission of an RPC request to the server.
         * \param request
         *      The RPC request payload to send. The caller must not modify or
         *      even access \a request until notifier's completed or failed
         *      method has been invoked.
         * \param[out] response
         *      A Buffer that will be filled in with the RPC response. The
         *      transport will clear any existing contents.  The caller must
         *      not access \a response until \a notifier's \c completed or
         *      \c failed method has been invoked.
         * \param notifier
         *      This object will be invoked when the RPC has completed or
         *      failed. It also serves as a unique identifier for the RPC,
         *      which can be passed to cancelRequest.
         */
        virtual void sendRequest(Buffer *request, Buffer *response,
                                 RpcNotifier *notifier) {}

        /**
         * Cancel an RPC request that was sent previously.
         * \param notifier
         *      Notifier object associated with this request (was passed
         *      to #sendRequest when the RPC was initiated).
         */
        virtual void cancelRequest(RpcNotifier *notifier) {}

        /**
         * Returns a human-readable string containing useful information
         * about any active RPC(s) on this session.
         */
        virtual std::string getRpcInfo() {
            return format("unknown RPC(s) on %s", serviceLocator.c_str());
        }

        /**
         * Shut down this session: abort any RPCs in progress and reject
         * any future calls to \c sendRequest. The caller is responsible
         * for logging the reason for the abort.
         */
        virtual void abort() {}

        friend void intrusive_ptr_add_ref(Session *session);

        friend void intrusive_ptr_release(Session *session);

    protected:
        std::atomic<int> refCount;      /// Count of SessionRefs that exist
        /// for this Session.

    public:
        /// The service locator this Session is connected to.
        const std::string serviceLocator;

    };

    typedef std::shared_ptr<Session> SessionRef;

    /**
     * Constructor for Transport.
     */
    Transport() = default;

    /**
     * Destructor for Transport.
     */
    virtual ~Transport() = default;

    /**
     * Return a session that will communicate with the given service locator.
     * This function is normally only invoked by TransportManager; clients
     * call TransportManager::getSession.
     *
     * \param serviceLocator
     *      Identifies the server this session will communicate with.
     * \param timeoutMs
     *      If the server becomes nonresponsive for this many milliseconds
     *      the session will be aborted.  0 means the session will pick a
     *      suitable default value.
     * \throw NoSuchKeyException
     *      Service locator option missing.
     * \throw BadValueException
     *      Service locator option malformed.
     * \throw TransportException
     *      The transport can't open a session for \a serviceLocator.
     */
    virtual SessionRef getSession(const std::string *serviceLocator,
                                  uint32_t timeoutMs) = 0;

    /**
     * Return the ServiceLocator for this Transport. If the transport
     * was not provided static parameters (e.g. fixed TCP or UDP port),
     * this function will return a SerivceLocator with those dynamically
     * allocated attributes.
     *
     * Enlisting the dynamic ServiceLocator with the Coordinator permits
     * other hosts to contact dynamically addressed services.
     */
    virtual std::string getServiceLocator() = 0;
};

}


#endif //GUNGNIR_TRANSPORT_H
