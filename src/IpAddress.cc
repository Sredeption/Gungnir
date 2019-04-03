#include "IpAddress.h"

#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>


namespace Gungnir {

IpAddress::IpAddress(const std::string &serviceLocator) : address() {
    try {
        hostent host{};
        hostent *result;
        char buffer[4096];
        int error;
        sockaddr_in *addr = reinterpret_cast<sockaddr_in *>(&address);
        addr->sin_family = AF_INET;

        auto delimiter = serviceLocator.find(':');

        std::string hostName = serviceLocator.substr(0, delimiter);
        uint16_t port = std::stoi(serviceLocator.substr(delimiter + 1, serviceLocator.length()));

        // Warning! The return value from getthostbyname_r is advertised
        // as being the same as what is returned at error, but it is not;
        // don't use it.
        gethostbyname_r(hostName.c_str(), &host, buffer, sizeof(buffer),
                        &result, &error);
        if (result == nullptr) {
            // If buffer is too small, an error value of ERANGE is supposed
            // to be returned, but in fact it appears that error is -1 in
            // the situation; check for both.
            if ((error == ERANGE) || (error == -1)) {
                throw FatalError(HERE,
                                 "IpAddress::IpAddress called gethostbyname_r"
                                 " with too small a buffer");
            }
            throw BadIpAddressException(HERE,
                                        std::string("couldn't find host '") +
                                        hostName + "'", serviceLocator);
        }
        memcpy(&addr->sin_addr, host.h_addr, sizeof(addr->sin_addr));
        addr->sin_port = HTONS(port);
    } catch (...) {
        throw BadIpAddressException(HERE, "Parse error", serviceLocator);
    }
}

IpAddress::IpAddress(const sockaddr *address) : address(*address) {}

std::string IpAddress::toString() const {
    const auto *addr = reinterpret_cast<const sockaddr_in *>(&address);
    uint32_t ip = ntohl(addr->sin_addr.s_addr);
    uint32_t port = NTOHS(addr->sin_port);
    return format("%d.%d.%d.%d:%d", (ip >> 24) & 0xff,
                  (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff, port);
}
}
