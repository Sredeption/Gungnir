#include "Server.h"
#include "Dispatch.h"

namespace Gungnir {

Server::Server(Context *context) :
    context(context) {

}

void Server::run() {

    Dispatch& dispatch = *context->dispatch;

    dispatch.run();
}
}
