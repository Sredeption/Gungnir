#include <cstdarg>
#include <cassert>
#include <cxxabi.h>
#include <fcntl.h>
#include <unistd.h>

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


uint64_t generateRandom() {
    // Internal scratch state used by random_r 128 is the same size as
    // initstate() uses for regular random(), see manpages for details.
    // statebuf is malloc'ed and this memory is leaked, it could be a __thread
    // buffer, but after running into linker issues with large thread local
    // storage buffers, we thought better.
    enum {
        STATE_BYTES = 128
    };
    static __thread char *statebuf;
    // random_r's state, must be handed to each call, and seems to refer to
    // statebuf in some undocumented way.
    static __thread random_data buf;

    if (statebuf == nullptr) {
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd < 0)
            throw FatalError(HERE, "Couldn't open /dev/urandom", errno);
        unsigned int seed;
        ssize_t bytesRead = read(fd, &seed, sizeof(seed));
        close(fd);
        if (bytesRead != sizeof(seed)) {
            assert(bytesRead == sizeof(seed));
        }
        statebuf = static_cast<char *>(std::malloc(STATE_BYTES));
        initstate_r(seed, statebuf, STATE_BYTES, &buf);
    }

    // Each call to random returns 31 bits of randomness,
    // so we need three to get 64 bits of randomness.
    static_assert(RAND_MAX >= (1 << 31), "RAND_MAX too small");
    int32_t lo, mid, hi;
    random_r(&buf, &lo);
    random_r(&buf, &mid);
    random_r(&buf, &hi);
    uint64_t r = (((uint64_t(hi) & 0x7FFFFFFF) << 33) | // NOLINT
                  ((uint64_t(mid) & 0x7FFFFFFF) << 2) | // NOLINT
                  (uint64_t(lo) & 0x00000003)); // NOLINT
    return r;
}

uint32_t randomNumberGenerator(uint32_t n) {
    return static_cast<uint32_t>(generateRandom()) % n;
}

}

