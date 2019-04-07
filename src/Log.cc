#include "Log.h"
#include "Object.h"
#include "Exception.h"
#include "Logger.h"

#include <memory>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>

namespace Gungnir {

LogEntry::LogEntry(LogEntryType type, Key key)
    : type(type), key(key) {

}

Log::Log(const char *filePath) : head(nullptr), tail(nullptr), appendedLength(0), syncedLength(0), lock(), fd()
                                 , writer() {
    head = tail = new Segment();
    if (::access(filePath, F_OK) != -1) {

    } else {

    }
    fd = ::open(filePath, O_CREAT | O_RDWR | O_SYNC, 0666);
    if (fd == -1)
        throw FatalError(HERE, "log file create failed");
}

Log::~Log() {
    ::close(fd);
}

Log::Segment::Segment() : data(nullptr), length(0), writeOffset(0), next(nullptr) {
    data = (char *) std::malloc(SEGMENT_SIZE);
}

Log::Segment::~Segment() {
    std::free(data);
}

uint64_t Log::append(LogEntry *entry) {
    uint32_t syncLength;
    char *dest;
    {
        SpinLock::Guard guard(lock);
        uint32_t entryLength = entry->length();
        if (tail->length + entryLength > SEGMENT_SIZE) {
            tail->next = new Segment();
            tail = tail->next;
        }
        appendedLength += entryLength;
        syncLength = appendedLength;
        dest = tail->data + tail->length;

        tail->length += entryLength;
        entry->copyTo(dest);
    }

    return syncLength;
}

bool Log::sync(uint64_t toOffset) {
    return toOffset <= syncedLength;
}

bool Log::write() {
    bool workDone = false;
    uint64_t length = 0;
    char *buffer = nullptr;

    assert(head != nullptr);
    {
        SpinLock::Guard guard(lock);
        if (head->length > head->writeOffset) {
            buffer = head->data + head->writeOffset;
            length = head->length - head->writeOffset;

        }
    }

    if (length != 0) {
        workDone = true;
        length = ::write(fd, buffer, length);
        if (length == -1)
            throw FatalError(HERE, "write log error");

    }

    {
        SpinLock::Guard guard(lock);
        head->writeOffset += length;

        if (head != tail && head->writeOffset == head->length) {
            Segment *oldHead = head;
            head = head->next;
            delete oldHead;
        }
    }
    return workDone;
}

void Log::writerThread(Log *log) {
    while (true) {
        while (log->write());
        if (!log->write()) {
            useconds_t r = static_cast<useconds_t>(generateRandom() % POLL_USEC) / 10;
            usleep(r);
        }
    }
}

LogEntry *Log::read() {
    LogEntryType type;
    uint64_t key;
    uint32_t len;
    char buffer[SEGMENT_SIZE];
    ssize_t ret;

    ret = ::read(fd, &type, sizeof(type));
    if (ret <= 0)
        return nullptr;
    ret = ::read(fd, &key, sizeof(key));
    if (ret <= 0)
        return nullptr;
    switch (type) {
        case LOG_ENTRY_TYPE_OBJ:
            ret = ::read(fd, &len, sizeof(len));
            if (ret <= 0)
                return nullptr;

            ret = ::read(fd, buffer, len);
            if (ret <= 0)
                return nullptr;
            return new Object(key, buffer, len);
        case LOG_ENTRY_TYPE_OBJTOMB:
            return new ObjectTombstone(key);
        default:
            return nullptr;
    }
}

}
