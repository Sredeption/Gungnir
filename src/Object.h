#ifndef GUNGNIR_OBJECT_H
#define GUNGNIR_OBJECT_H

#include "Key.h"
#include "Buffer.h"
#include "Log.h"


namespace Gungnir {

class Object : public LogEntry {
public:
    Buffer value;

    Object(Key key, Buffer *value);

    Object(Key key, const void *data, uint32_t length);

    uint32_t length() override;

    void copyTo(char *dest) override;
};

class ObjectTombstone : public LogEntry {
public:
    explicit ObjectTombstone(Key key);

    uint32_t length() override;

    void copyTo(char *dest) override;
};

}

#endif //GUNGNIR_OBJECT_H
