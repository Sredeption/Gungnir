#ifndef GUNGNIR_OPTIONCONFIG_H
#define GUNGNIR_OPTIONCONFIG_H

#include <cxxopts.hpp>

namespace Gungnir {

class OptionConfig {

public:
    OptionConfig();

    void parse(int argc, char *argv[]);

private:
    cxxopts::Options options;
public:
    std::string serverLocator;
    std::string connectLocator;
    uint32_t maxCores;

};

}

#endif //GUNGNIR_OPTIONCONFIG_H
