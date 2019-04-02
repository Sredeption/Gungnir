#ifndef GUNGNIR_COMMON_H
#define GUNGNIR_COMMON_H


#include <string>

namespace Gungnir {

std::string format(const char *format, ...)
__attribute__((format(printf, 1, 2)));

std::string vformat(const char *format, va_list ap)
__attribute__((format(printf, 1, 0)));

std::string demangle(const char *name);
}


#endif //GUNGNIR_COMMON_H
