#include "Log.h"
#include "Object.h"
#include "Exception.h"

#include <memory>
#include <cstring>
#include <cassert>

namespace Gungnir {

LogEntry::LogEntry(LogEntryType type, Key key)
    : type(type), key(key) {

}

Log::Log() : head(nullptr), tail(nullptr), appendedLength(0), syncedLength(0), lock(), fd()
             , cbs(), writer() {
    head = tail = new Segment();
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
        appendedLength += entryLength;
        syncLength = appendedLength;

        if (tail->length + entryLength > SEGMENT_SIZE) {
            tail->next = new Segment();
            tail = tail->next;
        }
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
    Segment *segment = head;
    int count = 0;
    memset(cbs, 0, sizeof(cbs));
    uint64_t writeLength = 0;

    {
        SpinLock::Guard guard(lock);
        while (segment != nullptr) {
            if (segment->length > segment->writeOffset) {
                aiocb *cb = cbs + count;
                cb->aio_fildes = fd;
                cb->aio_offset = syncedLength;
                cb->aio_buf = segment->data + segment->writeOffset;
                uint64_t segmentWriteLength = segment->length - segment->writeOffset;
                cb->aio_nbytes = segmentWriteLength;
                writeLength += segmentWriteLength;
                aio_write(cb);
                count++;
                if (count >= 10)
                    break;
            }
            segment = segment->next;
        }
    }

    for (int i = 0; i < count; i++) {
        aiocb *cb = cbs + i;
        aio_suspend(&cb, 1, nullptr);
        ssize_t r = aio_return(cb);
        if (r == -1) {
            throw FatalError(HERE, "Failed to write log");
        } else if (r != cb->aio_nbytes) {
            throw FatalError(HERE, "Unexpectedly short write");
        }
    }

    {
        SpinLock::Guard guard(lock);
        segment = head;
        while (writeLength > 0) {
            assert(segment!= nullptr);
            uint64_t segmentWriteLength = segment->length - segment->writeOffset;
            segmentWriteLength = std::min(segmentWriteLength, writeLength);
            segment->writeOffset += segmentWriteLength;
            writeLength -= segmentWriteLength;
            if (head != tail && segment->writeOffset == segment->length) {
                Segment *oldHead = head;
                head = head->next;
                delete oldHead;
            }
        }
    }
    return false;
}

void Log::writerThread(Log *log) {
    while (true) {
        log->write();
    }
}
}
