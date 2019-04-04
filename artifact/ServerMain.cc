#include <Exception.h>
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
    Logger::log("server listen to %s", optionConfig.serverLocator.c_str());

    Context context(optionConfig, true);
    return 0;
}