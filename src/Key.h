//
// Created by issac on 4/2/19.
//

#ifndef GUNGNIR_KEY_H
#define GUNGNIR_KEY_H


#include <cstdint>

namespace Gungnir {

class Key {
public:
    Key();

    Key(uint64_t key);

    uint64_t value() const;

private:
    uint64_t key;

};

}


#endif //GUNGNIR_KEY_H
