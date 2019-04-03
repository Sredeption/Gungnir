#include "Key.h"

namespace Gungnir {

Key::Key() : Key(0) {

}

Key::Key(uint64_t key) : key(key) {
}

uint64_t Key::value() const {
    return key;
}

}
