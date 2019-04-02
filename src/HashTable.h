#ifndef GUNGNIR_HASHTABLE_H
#define GUNGNIR_HASHTABLE_H


#include "Logger.h"

namespace Gungnir {

class HashTable {
public:
    HashTable(){
        Logger::log(HERE, "hash table initialized");
    }
};

}

#endif //GUNGNIR_HASHTABLE_H
