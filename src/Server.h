#ifndef GUNGNIR_SERVER_H
#define GUNGNIR_SERVER_H

#include "Context.h"

namespace Gungnir {

class Server {

public:
    explicit Server(Context *context);

    void run();
private:
    Context *context;
};

}

#endif //GUNGNIR_SERVER_H
