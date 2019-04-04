#include "OptionConfig.h"

#include <thread>

namespace Gungnir {

OptionConfig::OptionConfig() :
    options("Gungnir", "High performance key value store")
    , serverLocator(), connectLocator(), maxCores(1) {
    maxCores = std::max(maxCores, std::thread::hardware_concurrency() / 2);
    options.add_options()
        ("l,listen", "Server listening address", cxxopts::value<std::string>(serverLocator))
        ("c,connect", "Client connect address", cxxopts::value<std::string>(connectLocator))
        ("C,maxCores", "Max core number", cxxopts::value<uint32_t>(maxCores));
}

void OptionConfig::parse(int argc, char **argv) {
    options.parse(argc, argv);
}

}
