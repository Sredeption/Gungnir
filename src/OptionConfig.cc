#include "OptionConfig.h"

namespace Gungnir {

OptionConfig::OptionConfig() : options("Gungnir", "High performance key value store") {

    options.add_options()
        ("l,listen", "Server listening address", cxxopts::value<std::string>(serverLocator))
        ("c,connect", "Client connect address", cxxopts::value<std::string>(connectLocator));
}

void OptionConfig::parse(int argc, char **argv) {
    options.parse(argc, argv);
}

}
