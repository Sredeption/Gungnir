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
};

class ObjectTombstone : public LogEntry {
public:
    explicit ObjectTombstone(Key key);
};

}

#endif //GUNGNIR_OBJECT_H
