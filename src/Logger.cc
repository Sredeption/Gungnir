#include <cstdarg>
#include "Logger.h"

namespace Gungnir {

void Logger::log(const CodeLocation &where, const char *fmt, ...) {
    printf("%s:%d %s: ", where.file, where.line, where.function);
    va_list ap;
    va_start(ap, fmt);
    printf(fmt, ap);
    va_end(ap);
}

}
