
#include "Object.h"

namespace Gungnir {


Object::Object(Key key, Buffer *value)
    : LogEntry(LOG_ENTRY_TYPE_OBJ, key) {
    this->value.append(value);

}

ObjectTombstone::ObjectTombstone(Key key)
    : LogEntry(LOG_ENTRY_TYPE_OBJTOMB, key) {

}
}
