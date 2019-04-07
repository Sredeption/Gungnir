
#include <cstring>
#include "Object.h"

namespace Gungnir {


Object::Object(Key key, Buffer *value)
    : LogEntry(LOG_ENTRY_TYPE_OBJ, key) {
    this->value.append(value);

}

uint32_t Object::length() {
    return sizeof(type) + sizeof(key) + sizeof(uint32_t) + value.size();
}

void Object::copyTo(char *dest) {
    uint64_t key = this->key.value();
    uint32_t len = length();

    memcpy(dest, &type, 1);
    memcpy(dest + 1, &key, 8);
    memcpy(dest + 9, &len, 4);
    memcpy(dest + 13, value.getStart<char>(), value.size());
}

ObjectTombstone::ObjectTombstone(Key key)
    : LogEntry(LOG_ENTRY_TYPE_OBJTOMB, key) {

}

uint32_t ObjectTombstone::length() {
    return sizeof(type) + sizeof(key);
}

void ObjectTombstone::copyTo(char *dest) {
    uint64_t key = this->key.value();
    uint32_t size = length();
    memcpy(dest, &type, 1);
    memcpy(dest + 1, &key, 8);
}
}
