#include <Exception.h>
#include <Server.h>
#include "Context.h"
#include "Logger.h"
#include "OptionConfig.h"

using namespace Gungnir;

int main(int argc, char *argv[]) {

    OptionConfig optionConfig;
    optionConfig.parse(argc, argv);

    if (optionConfig.serverLocator.empty()) {
        throw FatalError(HERE, "No listening address specified");
    }
    Logger::log("server listen on %s", optionConfig.serverLocator.c_str());

    Context context(optionConfig, true);

    Server server(&context);
    server.run();
    return 0;
}