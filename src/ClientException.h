#ifndef GUNGNIR_CLIENTEXCEPTION_H
#define GUNGNIR_CLIENTEXCEPTION_H

#include "Exception.h"
#include "WireFormat.h"

namespace Gungnir {

/**
 * The base class for all exceptions that can be generated within
 * clients by the Gungnir library. Exceptions correspond to Status
 * values. Within the server, any of these exceptions may be thrown
 * at any time, which will abort the request and reflect the exception
 * back to the client.
 */
class ClientException : public std::exception {
public:
    ClientException(const CodeLocation &where, Status status);

    ClientException(const ClientException &other);

    ClientException &operator=(const ClientException &other);

    ~ClientException() noexcept override;

    static void throwException(const CodeLocation &where, Status status)
    __attribute__((noreturn));

    std::string str() const;

    const char *what() const noexcept override;

    /**
     * Describes a problem that prevented normal completion of a
     * RAMCloud operation.
     */
    Status status;

    CodeLocation where;
private:
    mutable std::unique_ptr<const char[]> whatCache;
};

class InternalError : public ClientException {
public:
    InternalError(const CodeLocation &where, Status status)
        : ClientException(where, status) {}
};

class RetryException : public ClientException {
public:

    explicit RetryException(const CodeLocation &where, uint32_t minDelayMicros = 0,
                            uint32_t maxDelayMicros = 0, const char *message = nullptr)
        : ClientException(where, STATUS_RETRY)
          , minDelayMicros(minDelayMicros)
          , maxDelayMicros(maxDelayMicros)
          , message(message) {}

    RetryException(const RetryException &other)
        : ClientException(other)
          , minDelayMicros(other.minDelayMicros)
          , maxDelayMicros(other.maxDelayMicros)
          , message(other.message) {}

    RetryException &operator=(const RetryException &other) {
        ClientException::operator=(other);
        minDelayMicros = other.minDelayMicros;
        maxDelayMicros = other.maxDelayMicros;
        message = other.message;
        return *this;
    }

    /// Copies of the constructor arguments.
    uint32_t minDelayMicros;
    uint32_t maxDelayMicros;
    const char *message;
};

#define DEFINE_EXCEPTION(name, status, superClass)          \
class name : public superClass {                            \
  public:                                                   \
    explicit name(const CodeLocation& where)                \
        : superClass(where, status) { }                     \
};

DEFINE_EXCEPTION(Success,
                 STATUS_OK,
                 ClientException)

DEFINE_EXCEPTION(ObjectDoesntExistException,
                 STATUS_OBJECT_DOESNT_EXIST,
                 ClientException)

DEFINE_EXCEPTION(MessageErrorException,
                 STATUS_MESSAGE_ERROR ,
                 InternalError)
}

#endif //GUNGNIR_CLIENTEXCEPTION_H
