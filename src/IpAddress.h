#ifndef GUNGNIR_IPADDRESS_H
#define GUNGNIR_IPADDRESS_H

#include "Exception.h"

#include <netinet/in.h>
#include <string>

namespace Gungnir {

class IpAddress {
public:
    class BadIpAddressException : public Exception {
    public:
        /**
         * Construct a BadIpAddressException.
         * \param where
         *      Pass #HERE here.
         * \param msg
         *      String describing the problem; should start with a
         *      lower-case letter.
         * \param serviceLocator
         *      The ServiceLocator that couldn't be parsed: used to
         *      generate a prefix message containing the original locator
         *      string.
         */
        explicit BadIpAddressException(
            const CodeLocation &where,
            const std::string &msg,
            const std::string &serviceLocator) :
            Exception(where,
                      "Service locator '" +
                      serviceLocator +
                      "' couldn't be converted to IP address: " + msg) {}
    };

    explicit IpAddress(const std::string &serviceLocator);

    explicit IpAddress(const sockaddr *address);

    std::string toString() const;

    sockaddr address;
};

}

#endif //GUNGNIR_IPADDRESS_H
