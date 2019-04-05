#include "OptionConfig.h"

#include <thread>

namespace Gungnir {

OptionConfig::OptionConfig() :
    options("Gungnir", "High performance key value store")
    , serverLocator(), connectLocator(), maxCores(1), readPercent(50), targetOps(1000000), objectCount(10000000)
    , objectSize(128), time(2) {
    maxCores = std::max(maxCores, std::thread::hardware_concurrency() / 2);
    options.add_options()
        ("l,listen", "Server listening address", cxxopts::value<std::string>(serverLocator))
        ("c,connect", "Client connect address", cxxopts::value<std::string>(connectLocator))
        ("C,maxCores", "Max core number", cxxopts::value<uint32_t>(maxCores))
        ("readPercent", "Read percentage of YCSB workload", cxxopts::value<uint32_t>(readPercent))
        ("targetOps", "Target throughput(op/s) of YCSB workload", cxxopts::value<uint64_t>(targetOps))
        ("objectCount", "Maximum object number of YCSB workload", cxxopts::value<uint32_t>(objectCount))
        ("objectSize", "Maximum object size of YCSB workload", cxxopts::value<uint32_t>(objectSize))
        ("time", "Benchmark time of YCSB workload", cxxopts::value<uint64_t>(time));
}

void OptionConfig::parse(int argc, char **argv) {
    options.parse(argc, argv);
}

}
