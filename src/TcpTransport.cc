#include <memory>

#include <memory>

#include <sys/ioctl.h>
#include <cstdio>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <climits>
#include <arpa/inet.h>

#include "TcpTransport.h"
#include "Logger.h"
#include "IpAddress.h"
#include "WorkerManager.h"

namespace Gungnir {

TcpTransport::TcpTransport(Context *context, const std::string &serviceLocator)
    : context(context), locatorString(serviceLocator), listenSocket(-1), acceptHandler(), sockets(), nextSocketId(100)
      , serverRpcPool(), clientRpcPool() {

    IpAddress address(serviceLocator);
    listenSocket = socket(PF_INET, SOCK_STREAM, 0);

    if (listenSocket == -1) {
        Logger::log(HERE, "TcpTransport couldn't create listen socket: %s", strerror(errno));
        throw TransportException(HERE, "TcpTransport couldn't create listen socket", errno);
    }


    int r = fcntl(listenSocket, F_SETFL, O_NONBLOCK);

    if (r != 0) {
        close(listenSocket);
        Logger::log(HERE, "TcpTransport couldn't set nonblocking on listen "
                          "socket: %s", strerror(errno));
        throw TransportException(
            HERE, "TcpTransport couldn't set nonblocking on listen socket", errno);
    }

    int optval = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) != 0) {
        close(listenSocket);
        Logger::log(HERE, "TcpTransport couldn't set SO_REUSEADDR on "
                          "listen socket: %s", strerror(errno));
        throw TransportException(
            HERE, "TcpTransport couldn't set SO_REUSEADDR on listen socket", errno);
    }

    if (bind(listenSocket, &address.address,
             sizeof(address.address)) == -1) {
        close(listenSocket);
        std::string message = format("TcpTransport couldn't bind to '%s'",
                                     serviceLocator.c_str());
        Logger::log(HERE, "%s: %s", message.c_str(), strerror(errno));
        throw TransportException(HERE, message, errno);
    }

    if (listen(listenSocket, INT_MAX) == -1) {
        close(listenSocket);
        Logger::log(HERE, "TcpTransport couldn't listen on socket: %s",
                    strerror(errno));
        throw TransportException(HERE,
                                 "TcpTransport couldn't listen on socket", errno);
    }

    // Arrange to be notified whenever anyone connects to listenSocket.
    acceptHandler = std::make_unique<AcceptHandler>(listenSocket, this);
}

TcpTransport::~TcpTransport() {
    if (listenSocket >= 0) {
        close(listenSocket);
        listenSocket = -1;
    }
    for (unsigned int i = 0; i < sockets.size(); i++) {
        if (sockets[i] != nullptr) {
            closeSocket(i);
        }
    }
}

Transport::SessionRef TcpTransport::getSession(const std::string &serviceLocator, uint32_t timeoutMs) {
    return std::make_shared<TcpSession>(this, serviceLocator, timeoutMs);
}

std::string TcpTransport::getServiceLocator() {
    return locatorString;
}

TcpTransport::IncomingMessage::IncomingMessage(Buffer *buffer, TcpTransport::TcpSession *session)
    : header(), headerBytesReceived(0), messageBytesReceived(0), messageLength(0), buffer(buffer), session(session) {

}

void TcpTransport::IncomingMessage::cancel() {
    buffer = nullptr;
    messageLength = 0;
}

/**
 * Attempt to read part or all of a message from an open socket.
 *
 * \param fd
 *      File descriptor to use for reading message info.
 * \return
 *      True means the message is complete (it's present in the
 *      buffer provided to the constructor); false means we still need
 *      more data.
 *
 * \throw TransportException
 *      An I/O error occurred.
 */
bool TcpTransport::IncomingMessage::readMessage(int fd) {
    // First make sure we have received the header (it may arrive in
    // multiple chunks).
    if (headerBytesReceived < sizeof(Header)) {
        ssize_t len = TcpTransport::recvCarefully(fd,
                                                  reinterpret_cast<char *>(&header) + headerBytesReceived,
                                                  sizeof(header) - headerBytesReceived);
        headerBytesReceived += static_cast<uint32_t>(len);
        if (headerBytesReceived < sizeof(Header))
            return false;

        // Header is complete; check for various errors and set up for
        // reading the body.
        messageLength = header.len;
        if (header.len > MAX_RPC_LEN) {
            Logger::log(HERE, "TcpTransport received oversize message (%d bytes); "
                              "discarding extra bytes", header.len);
            messageLength = MAX_RPC_LEN;
        }

        if ((buffer == nullptr) && (session != nullptr)) {
            buffer = session->findRpc(&header);
        }
        if (buffer == nullptr)
            messageLength = 0;
    }

    // We have the header; now receive the message body (it may take several
    // calls to this method before we get all of it).
    if (messageBytesReceived < messageLength) {
        void *dest;
        if (buffer->size() == 0) {
            dest = buffer->alloc(messageLength);
        } else {
            buffer->peek(messageBytesReceived, &dest);
        }
        ssize_t len = TcpTransport::recvCarefully(fd, dest,
                                                  messageLength - messageBytesReceived);
        messageBytesReceived += static_cast<uint32_t>(len);
        if (messageBytesReceived < messageLength)
            return false;
    }

    // We have the header and the message body, but we may have to discard
    // extraneous bytes.
    if (messageBytesReceived < header.len) {
        char buffer[4096];
        uint32_t maxLength = header.len - messageBytesReceived;
        if (maxLength > sizeof(buffer))
            maxLength = sizeof(buffer);
        ssize_t len = TcpTransport::recvCarefully(fd, buffer, maxLength);
        messageBytesReceived += static_cast<uint32_t>(len);
        if (messageBytesReceived < header.len)
            return false;
    }
    return true;
}

void TcpTransport::TcpServerRpc::sendReply() {
    try {
        Socket *socket = transport->sockets[fd];

        // It's possible that our fd has been closed (or even reused for a
        // new connection); if so, just discard the RPC without sending
        // a response.
        if ((socket != nullptr) && (socket->id == socketId)) {
            if (!socket->rpcsWaitingToReply.empty()) {
                // Can't transmit the response yet; the socket is backed up.
                socket->rpcsWaitingToReply.push_back(this);
                return;
            }

            // Try to transmit the response.
            socket->bytesLeftToSend = TcpTransport::sendMessage(
                fd, message.header.nonce, &replyPayload, -1);
            if (socket->bytesLeftToSend > 0) {
                socket->rpcsWaitingToReply.push_back(this);
                socket->ioHandler.setEvents(Dispatch::FileEvent::READABLE |
                                            Dispatch::FileEvent::WRITABLE);
                return;
            }
        }
    } catch (TransportException &e) {
        transport->closeSocket(fd);
    }

    // The whole response was sent immediately (this should be the
    // common case).  Recycle the RPC object.
    transport->serverRpcPool.destroy(this);
}

std::string TcpTransport::TcpServerRpc::getClientServiceLocator() {
    Socket *socket = transport->sockets[fd];
    return format("tcp:host=%s,port=%hu", inet_ntoa(socket->sin.sin_addr),
                  NTOHS(socket->sin.sin_port));
}

TcpTransport::TcpClientRpc::TcpClientRpc(Buffer *request, Buffer *response, Transport::RpcNotifier *notifier,
                                         uint64_t nonce)
    : request(request), response(response), notifier(notifier), nonce(nonce)
      , sent(false) {}

void TcpTransport::closeSocket(int fd) {
    delete sockets[fd];
    sockets[fd] = nullptr;
    close(fd);
}

ssize_t TcpTransport::recvCarefully(int fd, void *buffer, size_t length) {
    ssize_t actual = recv(fd, buffer, length, MSG_DONTWAIT);

    if (actual == 0) {
        throw TransportException(HERE, "session closed by peer");
    }
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
        return 0;
    }
    Logger::log(HERE, "TcpTransport recv error: %s", strerror(errno));
    throw TransportException(HERE, "TcpTransport recv error", errno);
}


int TcpTransport::sendMessage(int fd, uint64_t nonce, Buffer *payload, int bytesToSend) {
    assert(fd >= 0);

    Header header{};
    header.nonce = nonce;
    header.len = payload->size();
    int totalLength = static_cast<int>(sizeof(header) + header.len);
    if (bytesToSend < 0) {
        bytesToSend = totalLength;
    }
    int alreadySent = totalLength - bytesToSend;

    // Use an iovec to send everything in one kernel call: one iov
    // for header, the rest for payload.  Skip parts that have
    // already been sent.
    uint32_t iovecs = 1 + payload->getNumberChunks();
    struct iovec iov[iovecs];
    int offset;
    int iovecIndex;
    if (alreadySent < static_cast<int>(sizeof(header))) {
        iov[0].iov_base = reinterpret_cast<char *>(&header) + alreadySent;
        iov[0].iov_len = sizeof(header) - alreadySent;
        iovecIndex = 1;
        offset = 0;
    } else {
        iovecIndex = 0;
        offset = alreadySent - static_cast<int>(sizeof(header));
    }
    Buffer::Iterator iter(payload, offset, header.len - offset);
    while (!iter.isDone()) {
        iov[iovecIndex].iov_base = const_cast<void *>(iter.getData());
        iov[iovecIndex].iov_len = iter.getLength();
        ++iovecIndex;

        // There's an upper limit on the permissible number of iovecs in
        // one outgoing message. Unfortunately, this limit does not appear
        // to be defined publicly, so we make a guess here. If we hit the
        // limit, stop accumulating chunks for this message: the remaining
        // chunks will get tried in a future invocation of this method.
        if (iovecIndex >= 100) {
            break;
        }
        iter.next();
    }

    msghdr msg{};
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = iovecIndex;

    int r = static_cast<int>(sendmsg(fd, &msg,
                                     MSG_NOSIGNAL | MSG_DONTWAIT));
    if (r == bytesToSend) {
        return 0;
    }
    if (r == -1) {
        if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
            Logger::log(HERE, "TcpTransport sendmsg error: %s", strerror(errno));
            throw TransportException(HERE, "TcpTransport sendmsg error",
                                     errno);
        }
        r = 0;
    }
    return bytesToSend - r;
}

TcpTransport::AcceptHandler::AcceptHandler(int fd, TcpTransport *transport)
    : Dispatch::File(transport->context->dispatch, fd, Dispatch::FileEvent::READABLE), transport(transport) {


}

void TcpTransport::AcceptHandler::handleFileEvent(uint32_t events) {
    struct sockaddr_in sin{};
    socklen_t socklen = sizeof(sin);

    int acceptedFd = accept(transport->listenSocket,
                            reinterpret_cast<sockaddr *>(&sin),
                            &socklen);
    if (acceptedFd < 0) {
        switch (errno) {
            // According to the man page for accept, you're supposed to
            // treat these as retry on Linux.
            case EHOSTDOWN:
            case EHOSTUNREACH:
            case ENETDOWN:
            case ENETUNREACH:
            case ENONET:
            case ENOPROTOOPT:
            case EOPNOTSUPP:
            case EPROTO:
                return;

                // No incoming connections are currently available.
            case EAGAIN:
#if EAGAIN != EWOULDBLOCK
                case EWOULDBLOCK:
#endif
                return;
        }

        // Unexpected error: log a message and then close the socket
        // (so we don't get repeated errors).
        Logger::log(HERE, "error in TcpTransport::AcceptHandler accepting "
                          "connection for '%s': %s",
                    transport->locatorString.c_str(), strerror(errno));
        setEvents(0);
        close(transport->listenSocket);
        transport->listenSocket = -1;
        return;
    }

    // Disable the hideous Nagle algorithm, which will delay sending small
    // messages in some situations (before adding this code in 5/2015, we
    // observed occasional 40ms delays when a server responded to a batch
    // of requests from the same client).
    int flag = 1;
    setsockopt(acceptedFd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    // At this point we have successfully opened a client connection.
    // Save information about it and create a handler for incoming
    // requests.
    if (transport->sockets.size() <=
        static_cast<unsigned int>(acceptedFd)) {
        transport->sockets.resize(acceptedFd + 1);
    }
    transport->sockets[acceptedFd] = new Socket(acceptedFd, transport, sin);
}

TcpTransport::ServerSocketHandler::ServerSocketHandler(int fd, TcpTransport *transport, TcpTransport::Socket *socket)
    : Dispatch::File(transport->context->dispatch, fd, Dispatch::FileEvent::READABLE)
      , fd(fd), transport(transport), socket(socket) {
}

void TcpTransport::ServerSocketHandler::handleFileEvent(uint32_t events) {
    // The following variables are copies of data from this object;
    // they are needed to safely detect socket closure below.
    TcpTransport *transport = this->transport;
    int socketFd = fd;
    Socket *socket = transport->sockets[socketFd];
    assert(socket != nullptr);
    try {
        if (events & Dispatch::FileEvent::READABLE) {
            if (socket->rpc == nullptr) {
                socket->rpc = transport->serverRpcPool.construct(socket,
                                                                 fd, transport);
            }
            if (socket->rpc->message.readMessage(fd)) {
                // The incoming request is complete; pass it off for servicing.
                TcpServerRpc *rpc = socket->rpc;
                socket->rpc = nullptr;
                transport->context->workerManager->handleRpc(rpc);
            }
        }
        // Check to see if this socket got closed due to an error in the
        // read handler; if so, it's neither necessary nor safe to continue
        // in this method. Note: the check for closure must be done without
        // accessing any fields in this object, since the object may have been
        // destroyed and its memory recycled.
        if (socket != transport->sockets[socketFd]) {
            return;
        }
        if (events & Dispatch::FileEvent::WRITABLE) {
            while (true) {
                if (socket->rpcsWaitingToReply.empty()) {
                    setEvents(Dispatch::FileEvent::READABLE);
                    break;
                }
                TcpServerRpc *rpc = socket->rpcsWaitingToReply.front();
                socket->bytesLeftToSend = TcpTransport::sendMessage(
                    fd, rpc->message.header.nonce, &rpc->replyPayload,
                    socket->bytesLeftToSend);
                if (socket->bytesLeftToSend != 0) {
                    break;
                }
                // The current reply is finished; start the next one, if
                // there is one.
                socket->rpcsWaitingToReply.pop_front();
                transport->serverRpcPool.destroy(rpc);
                socket->bytesLeftToSend = -1;
            }
        }
    } catch (TransportException &e) {
        transport->closeSocket(fd);
    }
}

TcpTransport::ClientSocketHandler::ClientSocketHandler(int fd, TcpTransport::TcpSession *session)
    : Dispatch::File(session->transport->context->dispatch, fd,
                     Dispatch::FileEvent::READABLE)
      , fd(fd), session(session) {

}

void TcpTransport::ClientSocketHandler::handleFileEvent(uint32_t events) {
    try {
        if (events & Dispatch::FileEvent::READABLE) {
            if (session->message->readMessage(fd)) {
                // This RPC is finished.
                if (session->current != nullptr) {
                    session->rpcsWaitingForResponse.remove(session->current);
                    session->current->notifier->completed();
                    session->transport->clientRpcPool.destroy(session->current);
                    session->current = nullptr;
                }
                session->message = std::make_unique<IncomingMessage>(static_cast<Buffer *>(nullptr), session);
            }
        }
        if (events & Dispatch::FileEvent::WRITABLE) {
            while (!session->rpcsWaitingToSend.empty()) {
                TcpClientRpc *rpc = session->rpcsWaitingToSend.front();
                session->bytesLeftToSend = TcpTransport::sendMessage(
                    session->fd, rpc->nonce, rpc->request,
                    session->bytesLeftToSend);
                if (session->bytesLeftToSend != 0) {
                    return;
                }
                // The current RPC is finished; start the next one, if
                // there is one.
                session->rpcsWaitingToSend.pop_front();
                session->rpcsWaitingForResponse.push_back(rpc);
                rpc->sent = true;
                session->bytesLeftToSend = -1;
            }
            setEvents(Dispatch::FileEvent::READABLE);
        }
    } catch (TransportException &e) {
        session->abort();
    }
}

TcpTransport::TcpSession::TcpSession(TcpTransport *transport, const std::string &serviceLocator, uint32_t timeoutMs)
    : Session(serviceLocator)
      , transport(transport), address(serviceLocator), fd(-1), serial(1), rpcsWaitingToSend(), bytesLeftToSend(0)
      , rpcsWaitingForResponse(), current(nullptr), message(), clientIoHandler() {
    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        Logger::log(HERE, "TcpTransport couldn't open socket for session: %s",
                    strerror(errno));
        throw TransportException(HERE,
                                 "TcpTransport couldn't open socket for session", errno);
    }

    int r = connect(fd, &address.address, sizeof(address.address));
    if (r == -1) {
        ::close(fd);
        fd = -1;
        Logger::log(HERE, "TcpTransport couldn't connect to %s: %s",
                    this->serviceLocator.c_str(), strerror(errno));
        throw TransportException(HERE, format(
            "TcpTransport couldn't connect to %s",
            this->serviceLocator.c_str()), errno);
    }

    // Check to see if we accidentally connected to ourself. This can
    // happen if the target server is on the same machine and has
    // crashed, so that it is no longer using its port. If this
    // happens our local socket (fd) might end up reusing that same port,
    // in which case we will connect to ourselves. If this happens,
    // abort this connection (it will get retried, at which point a
    // different port will get selected).
    sockaddr cAddr;
    socklen_t cAddrLen = sizeof(cAddr);
    // Read address information associated with our local socket.
    if (getsockname(fd, &cAddr, &cAddrLen)) {
        ::close(fd);
        fd = -1;
        Logger::log(HERE, "TcpTransport failed to get client socket info");
        throw TransportException(HERE,
                                 "TcpTransport failed to get client socket info", errno);
    }
    IpAddress sourceIp(&cAddr);
    IpAddress destinationIp(serviceLocator);
    if (sourceIp.toString() == destinationIp.toString()) {
        ::close(fd);
        fd = -1;
        Logger::log(HERE, "TcpTransport connected to itself %s",
                    sourceIp.toString().c_str());
        throw TransportException(HERE, format(
            "TcpTransport connected to itself %s",
            sourceIp.toString().c_str()));
    }

    // Disable the hideous Nagle algorithm, which will delay sending small
    // messages in some situations.
    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    /// Arrange for notification whenever the server sends us data.
    Dispatch::Lock lock(transport->context->dispatch);
    clientIoHandler = std::make_unique<ClientSocketHandler>(fd, this);
    message = std::make_unique<IncomingMessage>(static_cast<Buffer *>(nullptr), this);
}

TcpTransport::TcpSession::~TcpSession() {
    close();
}

void TcpTransport::TcpSession::abort() {
    close();
}

void TcpTransport::TcpSession::cancelRequest(Transport::RpcNotifier *notifier) {
    // Search for an RPC that refers to this notifier; if one is
    // found then remove all state relating to it.
    for (auto rpc = rpcsWaitingForResponse.begin(); rpc != rpcsWaitingForResponse.end(); rpc++) {

        if ((*rpc)->notifier == notifier) {
            rpcsWaitingForResponse.erase(rpc);
            transport->clientRpcPool.destroy(*rpc);

            // If we have started reading the response message,
            // cancel that also.
            if (*rpc == current) {
                message->cancel();
                current = nullptr;
            }
            return;
        }
    }

    for (auto rpc = rpcsWaitingToSend.begin(); rpc != rpcsWaitingToSend.end(); rpc++) {
        if ((*rpc)->notifier == notifier) {
            rpcsWaitingToSend.erase(rpc);
            transport->clientRpcPool.destroy(*rpc);
            return;
        }
    }
}

Buffer *TcpTransport::TcpSession::findRpc(TcpTransport::Header *header) {
    for (TcpClientRpc *rpc: rpcsWaitingForResponse) {
        if (rpc->nonce == header->nonce) {
            current = rpc;
            return rpc->response;
        }
    }
    return nullptr;
}

std::string TcpTransport::TcpSession::getRpcInfo() {
    return "TcpSession";
}

void TcpTransport::TcpSession::sendRequest(Buffer *request, Buffer *response, Transport::RpcNotifier *notifier) {
    response->reset();
    if (fd == -1) {
        notifier->failed();
        return;
    }

    TcpClientRpc *rpc = transport->clientRpcPool.construct(request, response, notifier, serial);
    serial++;
    if (!rpcsWaitingToSend.empty()) {
        // Can't transmit this request yet; there are already other
        // requests that haven't yet been sent.
        rpcsWaitingToSend.push_back(rpc);
        return;
    }

    // Try to transmit the request.
    try {
        bytesLeftToSend = TcpTransport::sendMessage(fd, rpc->nonce,
                                                    request, -1);
    } catch (TransportException &e) {
        abort();
        notifier->failed();
        return;
    }
    if (bytesLeftToSend == 0) {
        // The whole request was sent immediately (this should be the
        // common case).
        rpcsWaitingForResponse.push_back(rpc);
        rpc->sent = true;
    } else {
        rpcsWaitingToSend.push_back(rpc);
        clientIoHandler->setEvents(Dispatch::FileEvent::READABLE |
                                   Dispatch::FileEvent::WRITABLE);
    }
}

void TcpTransport::TcpSession::close() {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
    while (!rpcsWaitingForResponse.empty()) {
        TcpClientRpc *rpc = rpcsWaitingForResponse.front();
        rpc->notifier->failed();
        rpcsWaitingForResponse.pop_front();
        transport->clientRpcPool.destroy(rpc);

    }
    while (!rpcsWaitingToSend.empty()) {
        TcpClientRpc *rpc = rpcsWaitingToSend.front();
        rpc->notifier->failed();
        rpcsWaitingToSend.pop_front();
        transport->clientRpcPool.destroy(rpc);
    }
    if (clientIoHandler) {
        Dispatch::Lock lock(transport->context->dispatch);
        clientIoHandler.reset();
    }
}

TcpTransport::Socket::Socket(int fd, TcpTransport *transport, sockaddr_in &sin)
    : transport(transport), id(transport->nextSocketId), rpc(NULL), ioHandler(fd, transport, this), rpcsWaitingToReply()
      , bytesLeftToSend(0), sin(sin) {
    transport->nextSocketId++;
}

TcpTransport::Socket::~Socket() {
    if (rpc != nullptr) {
        transport->serverRpcPool.destroy(rpc);
    }
    while (!rpcsWaitingToReply.empty()) {
        TcpServerRpc *rpc = rpcsWaitingToReply.front();
        rpcsWaitingToReply.pop_front();
        transport->serverRpcPool.destroy(rpc);
    }
}
}
