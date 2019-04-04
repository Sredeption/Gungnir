#ifndef GUNGNIR_TCPTRANSPORT_H
#define GUNGNIR_TCPTRANSPORT_H

#include <list>
#include <sys/socket.h>
#include <netinet/in.h>

#include "Transport.h"
#include "Dispatch.h"
#include "Context.h"
#include "IpAddress.h"
#include "ObjectPool.h"

namespace Gungnir {

class TcpTransport : public Transport {

public:
    explicit TcpTransport(Context *context, const std::string &serviceLocator);

    ~TcpTransport() override;

    SessionRef getSession(const std::string &serviceLocator) override;

    std::string getServiceLocator() override;

    class TcpServerRpc;

private:

    class ServerSocketHandler;

    class IncomingMessage;

    class ClientSocketHandler;

    class Socket;

    class TcpSession;

    friend class AcceptHandler;

    friend class ServerSocketHandler;

    /**
     * Header for request and response messages: precedes the actual data
     * of the message in all transmissions.
     */
    struct Header {
        /// Unique identifier for this RPC: generated on the client, and
        /// returned by the server in responses.  This field makes it
        /// possible for a client to have multiple outstanding RPCs to
        /// the same server.
        uint64_t nonce;

        /// The size in bytes of the payload (which follows immediately).
        /// Must be less than or equal to #MAX_RPC_LEN.
        uint32_t len;
    } __attribute__((packed));

    /**
     * Used to manage the receipt of a message (on either client or server)
     * using an event-based approach.
     */
    class IncomingMessage {
        friend class ServerSocketHandler;

        friend class TcpServerRpc;

    public:
        IncomingMessage(Buffer *buffer, TcpSession *session);

        void cancel();

        bool readMessage(int fd);

    private:
        Header header;

        /// The number of bytes of header that have been successfully
        /// received so far; 0 means the header has not yet been received;
        /// sizeof(Header) means the header is complete.
        uint32_t headerBytesReceived;

        /// Counts the number of bytes in the message body that have been
        /// received so far.
        uint32_t messageBytesReceived;

        /// The number of bytes of input message that we will actually retain
        /// (normally this is the same as header.len, but it may be less
        /// if header.len is illegally large or if the entire message is being
        /// discarded).
        uint32_t messageLength;

        /// Buffer in which incoming message will be stored (not including
        /// transport-specific header); NULL means we haven't yet started
        /// reading the response, or else the RPC was canceled after we
        /// started reading the response.
        Buffer *buffer;

        /// Session that will find the buffer to use for this message once
        /// the header has arrived (or NULL).
        TcpSession *session;

    };

public:
    /**
     * The TCP implementation of Transport::ServerRpc.
     */
    class TcpServerRpc : public Transport::ServerRpc {
        friend class ServerSocketHandler;

        friend class TcpTransport;

        friend class ObjectPool<TcpServerRpc>;

    public:
        ~TcpServerRpc() override = default;

        void sendReply() override;

        std::string getClientServiceLocator() override;

        TcpServerRpc(Socket *socket, int fd, TcpTransport *transport)
            : fd(fd), socketId(socket->id), message(&requestPayload, nullptr), transport(transport) {}

    private:
        int fd;
        uint64_t socketId;
        IncomingMessage message;
        TcpTransport *transport;

    };

    /**
     * The TCP implementation of Transport::ClientRpc.
     */
    class TcpClientRpc {
    public:
        friend class TcpTransport;

        friend class TcpSession;

        explicit TcpClientRpc(Buffer *request, Buffer *response,
                              RpcNotifier *notifier, uint64_t nonce);

    private:
        Buffer *request;          /// Request message for the RPC.
        Buffer *response;         /// Will eventually hold the response message.
        RpcNotifier *notifier;    /// Use this object to report completion.
        uint64_t nonce;           /// Unique identifier for this RPC; used
        /// to pair the RPC with its response.
        bool sent;                /// True means the request has been sent
    };

private:
    void closeSocket(int fd);

    static ssize_t recvCarefully(int fd, void *buffer, size_t length);

    static int sendMessage(int fd, uint64_t nonce, Buffer *payload,
                           int bytesToSend);

    class AcceptHandler : public Dispatch::File {
    public:
        AcceptHandler(int fd, TcpTransport *transport);

        void handleFileEvent(uint32_t events) override;

    private:
        // Transport that manages this socket.
        TcpTransport *transport;
    };

    class ServerSocketHandler : public Dispatch::File {
    public:
        ServerSocketHandler(int fd, TcpTransport *transport, Socket *socket);

        void handleFileEvent(uint32_t events) override;

    private:
        // The following variables are just copies of constructor arguments.
        int fd;
        TcpTransport *transport;
        Socket *socket;
    };

    class ClientSocketHandler : public Dispatch::File {
    public:
        ClientSocketHandler(int fd, TcpSession *session);

        void handleFileEvent(uint32_t events) override;

    private:
        // The following variables are just copies of constructor arguments.
        int fd;
        TcpSession *session;
    };

    class TcpSession : public Session {
        friend class ClientIncomingMessage;

        friend class TcpClientRpc;

        friend class ClientSocketHandler;

    public:
        explicit TcpSession(TcpTransport *transport, const std::string &serviceLocator);

        ~TcpSession() override;

        void abort() override;

        void cancelRequest(RpcNotifier *notifier) override;

        Buffer *findRpc(Header *header);

        std::string getRpcInfo() override;

        void sendRequest(Buffer *request, Buffer *response,
                         RpcNotifier *notifier) override;

    private:

        void close();

        TcpTransport *transport;
        IpAddress address;
        int fd;
        uint64_t serial;

        std::list<TcpClientRpc *> rpcsWaitingToSend;
        int bytesLeftToSend;      /// The number of (trailing) bytes in the
        std::list<TcpClientRpc *> rpcsWaitingForResponse;
        TcpClientRpc *current;
        std::unique_ptr<IncomingMessage> message;
        std::unique_ptr<ClientSocketHandler> clientIoHandler;
    };

    Context *context;

    std::string locatorString;

    /// File descriptor used by servers to listen for connections from
    /// clients.  -1 means this instance is not a server.
    int listenSocket;

    /// Used to wait for listenSocket to become readable.
    std::unique_ptr<AcceptHandler> acceptHandler;

    /// Used to hold information about a file descriptor associated with
    /// a socket, on which RPC requests may arrive.
    class Socket {
    public:
        Socket(int fd, TcpTransport *transport, sockaddr_in &sin);

        ~Socket();

        TcpTransport *transport;  /// The parent TcpTransport object.
        uint64_t id;              /// Unique identifier: no other Socket
        /// for this transport instance will use
        /// the same value.
        TcpServerRpc *rpc;        /// Incoming RPC that is in progress for
        /// this fd, or NULL if none.
        ServerSocketHandler ioHandler;
        /// Used to get notified whenever data
        /// arrives on this fd.
        std::list<TcpServerRpc *> rpcsWaitingToReply;
        /// RPCs whose response messages have not yet
        /// been transmitted.  The front RPC on this
        /// list is currently being transmitted.
        int bytesLeftToSend;      /// The number of (trailing) bytes in the
        /// front RPC on rpcsWaitingToReply that still
        /// need to be transmitted, once fd becomes
        /// writable again.  -1 or 0 means there are
        /// no RPCs waiting.
        struct sockaddr_in sin;   /// sockaddr_in of the client host on the
        /// other end of the socket. Used to
        /// implement #getClientServiceLocator().
    };

    /// Keeps track of all of our open client connections. Entry i has
    /// information about file descriptor i (NULL means no client
    /// is currently connected).
    std::vector<Socket *> sockets;

    /// Used to assign increasing id values to Sockets.
    uint64_t nextSocketId;

    /// Counts the number of nonzero-size partial messages sent by
    /// sendMessage (for testing only).
    static int messageChunks;

    /// Pool allocator for our ServerRpc objects.
    ObjectPool<TcpServerRpc> serverRpcPool;

    /// Pool allocator for TcpClientRpc objects.
    ObjectPool<TcpClientRpc> clientRpcPool;
};


}


#endif //GUNGNIR_TCPTRANSPORT_H
