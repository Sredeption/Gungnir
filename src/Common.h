#ifndef GUNGNIR_COMMON_H
#define GUNGNIR_COMMON_H


#include <string>

namespace Gungnir {
// htons, ntohs cause warnings
#define HTONS(x) \
    static_cast<uint16_t>((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))
#define NTOHS HTONS

std::string format(const char *format, ...)
__attribute__((format(printf, 1, 2)));

std::string vformat(const char *format, va_list ap)
__attribute__((format(printf, 1, 0)));

std::string demangle(const char *name);

uint64_t generateRandom();

uint32_t randomNumberGenerator(uint32_t n);
}


#endif //GUNGNIR_COMMON_H
