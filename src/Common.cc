#include <cstdarg>
#include <cassert>
#include <cxxabi.h>

#include "Common.h"
#include "CodeLocation.h"
#include "Exception.h"

namespace Gungnir {
/// A safe version of sprintf.
std::string format(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    std::string s = vformat(format, ap);
    va_end(ap);
    return s;
}

/// A safe version of vprintf.
std::string
vformat(const char *format, va_list ap) {
    std::string s;

    // We're not really sure how big of a buffer will be necessary.
    // Try 1K, if not the return value will tell us how much is necessary.
    int bufSize = 1024;
    while (true) {
        char buf[bufSize];
        // vsnprintf trashes the va_list, so copy it first
        va_list aq;
        __va_copy(aq, ap);
        int r = vsnprintf(buf, bufSize, format, aq);
        assert(r >= 0); // old glibc versions returned -1
        if (r < bufSize) {
            s = buf;
            break;
        }
        bufSize = r + 1;
    }

    return s;
}

std::string demangle(const char *name) {
    int status;
    char *res = abi::__cxa_demangle(name,
                                    NULL,
                                    NULL,
                                    &status);
    if (status != 0) {
        throw Gungnir::
        FatalError(HERE,
                   "cxxabi.h's demangle() could not demangle type");
    }
    // contruct a string with a copy of the C-style string returned.
    std::string ret(res);
    // __cxa_demangle would have used realloc() to allocate memory
    // which should be freed now.
    free(res);
    return ret;
}

}
