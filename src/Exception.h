#include <utility>

#ifndef GUNGNIR_EXCEPTION_H
#define GUNGNIR_EXCEPTION_H


#include <exception>
#include <cstring>
#include <memory>

#include "CodeLocation.h"

namespace Gungnir {

/**
 * The base class for all Gungnir exceptions.
 */
struct Exception : public std::exception {
    explicit Exception(const CodeLocation &where)
        : message(""), errNo(0), where(where), whatCache() {}

    Exception(const CodeLocation &where, std::string msg)
        : message(std::move(msg)), errNo(0), where(where), whatCache() {}

    Exception(const CodeLocation &where, int errNo)
        : message(""), errNo(errNo), where(where), whatCache() {
        message = strerror(errNo);
    }

    Exception(const CodeLocation &where, const std::string &msg, int errNo)
        : message(msg + ": " + strerror(errNo)), errNo(errNo), where(where), whatCache() {}

    Exception(const Exception &other)
        : message(other.message), errNo(other.errNo), where(other.where), whatCache() {}

    ~Exception() noexcept override = default;

    std::string str() const {
        return (demangle(typeid(*this).name()) + ": " + message +
                ", thrown at " + where.str());
    }

    const char *what() const noexcept override {
        if (whatCache)
            return whatCache.get();
        std::string s(str());
        char *cStr = new char[s.length() + 1];
        whatCache.reset(const_cast<const char *>(cStr));
        memcpy(cStr, s.c_str(), s.length() + 1);
        return cStr;
    }

    std::string message;
    int errNo;
    CodeLocation where;
private:
    mutable std::unique_ptr<const char[]> whatCache;
};

/**
 * A fatal error that should exit the program.
 */
struct FatalError : public Exception {
    explicit FatalError(const CodeLocation &where)
        : Exception(where) {}

    FatalError(const CodeLocation &where, std::string msg)
        : Exception(where, std::move(msg)) {}

    FatalError(const CodeLocation &where, int errNo)
        : Exception(where, errNo) {}

    FatalError(const CodeLocation &where, const std::string &msg, int errNo)
        : Exception(where, msg, errNo) {}
};

}

#endif //GUNGNIR_EXCEPTION_H
