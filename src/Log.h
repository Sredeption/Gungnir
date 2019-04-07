#ifndef GUNGNIR_LOG_H
#define GUNGNIR_LOG_H

#include <aio.h>
#include <memory>
#include <thread>

#include "Key.h"
#include "SpinLock.h"

namespace Gungnir {


enum LogEntryType : uint8_t {
    LOG_ENTRY_TYPE_OBJ,
    LOG_ENTRY_TYPE_OBJTOMB,
};

class LogEntry {
public:
    LogEntryType type;
    Key key;

    virtual uint32_t length() = 0;

    virtual void copyTo(char *dest) = 0;

protected:
    LogEntry(LogEntryType type, Key key);
};

class Log {
public:
    explicit Log(const char *filePath, bool recover, int segmentSize = 1024 * 1024);

    ~Log();

    void startWriter();

private:

    class Segment {
    public:
        char *data;
        uint64_t length;
        uint64_t writeOffset;
        Segment *next;

        Segment(int segmentSize);

        ~Segment();
    };


public:
    uint64_t append(LogEntry *entry);

    bool sync(uint64_t offset);

    bool write();

    LogEntry *read();

    Segment *head;
    Segment *tail;
    int segmentSize;
    uint64_t appendedLength;
    uint64_t syncedLength;
    SpinLock lock;

    int fd;
    std::unique_ptr<std::thread> writer;
    bool stopWriter;


    const static int POLL_USEC = 10000;

    static void writerThread(Log *log);
};

}

#endif //GUNGNIR_LOG_H
