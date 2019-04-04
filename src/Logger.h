#ifndef GUNGNIR_LOGGER_H
#define GUNGNIR_LOGGER_H


#include "CodeLocation.h"

namespace Gungnir {

class Logger {

public:
    static void log(const CodeLocation &where, const char *fmt, ...);

    static void log(const char *fmt, ...);

};

}


#endif //GUNGNIR_LOGGER_H
