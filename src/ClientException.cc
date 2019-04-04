#include "ClientException.h"

namespace Gungnir {

ClientException::ClientException(const CodeLocation &where, Status status)
    : status(status)
      , where(where)
      , whatCache() {
    // Constructor is empty.
}

ClientException::ClientException(const ClientException &other)
    : status(other.status)
      , where(other.where)
      , whatCache() {
    // Constructor is empty.
}

ClientException &
ClientException::operator=(const ClientException &other) {
    status = other.status;
    where = other.where;
    whatCache.release();
    return *this;
}

/**
 * Destructor for ClientExceptions.
 */
ClientException::~ClientException() noexcept {
    // Destructor is empty.
}

void
ClientException::throwException(const CodeLocation &where, Status status) {
    switch (status) {
        case STATUS_OK:
            // Not clear that this case really makes sense (throw
            // an exception to indicate success?) but it is here
            // for completeness.
            throw Success(where);
        case STATUS_OBJECT_DOESNT_EXIST:
            throw ObjectDoesntExistException(where);
        case STATUS_RETRY:
            throw RetryException(where);
        default:
            throw InternalError(where, status);
    }

}

std::string ClientException::str() const {
    return format("status:%d, thrown at %s", status, where.str().c_str());
}

const char *ClientException::what() const noexcept {
    if (whatCache)
        return whatCache.get();
    std::string s(str());
    char *cStr = new char[s.length() + 1];
    whatCache.reset(const_cast<const char *>(cStr));
    memcpy(cStr, s.c_str(), s.length() + 1);
    return cStr;
}


}
