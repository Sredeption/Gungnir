#ifndef GUNGNIR_LOG_H
#define GUNGNIR_LOG_H


#include "Key.h"

namespace Gungnir {


enum LogEntryType {
    LOG_ENTRY_TYPE_OBJ,
    LOG_ENTRY_TYPE_OBJTOMB,
};

class LogEntry {
protected:
    LogEntryType type;
    Key key;

protected:
    LogEntry(LogEntryType type, Key key);
};

class Log {

};

}

#endif //GUNGNIR_LOG_H
