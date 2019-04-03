#ifndef GUNGNIR_WORKERMANAGER_H
#define GUNGNIR_WORKERMANAGER_H

#include "Transport.h"

namespace Gungnir {

class WorkerManager {
public:
    void handleRpc(Transport::ServerRpc *rpc);
};

}


#endif //GUNGNIR_WORKERMANAGER_H
