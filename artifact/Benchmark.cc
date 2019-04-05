#include <Client.h>
#include <Logger.h>
#include <cmath>
#include <Cycles.h>
#include <vector>

using namespace Gungnir;

class ZipfianGenerator {
public:
    explicit ZipfianGenerator(uint64_t n, double theta = 0.99)
        : n(n), theta(theta), alpha(1 / (1 - theta)), zetan(zeta(n, theta)), eta(
        (1 - std::pow(2.0 / static_cast<double>(n), 1 - theta)) /
        (1 - zeta(2, theta) / zetan)) {}

    uint64_t nextNumber() {
        double u = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
        double uz = u * zetan;
        if (uz < 1)
            return 0;
        if (uz < 1 + std::pow(0.5, theta))
            return 1;
        return 0 + static_cast<uint64_t>(static_cast<double>(n) * std::pow(eta * u - eta + 1.0, alpha));
    }

private:
    const uint64_t n;
    const double theta;
    const double alpha;
    const double zetan;
    const double eta;

    static double zeta(uint64_t n, double theta) {
        double sum = 0;
        for (uint64_t i = 0; i < n; i++) {
            sum = sum + 1.0 / (std::pow(i + 1, theta));
        }
        return sum;
    }
};

enum SampleType {
    GET, PUT, ERASE
};

struct Sample {
    uint64_t startTicks;
    uint64_t endTicks;
    SampleType type;

    Sample(uint64_t startTicks,
           uint64_t endTicks,
           SampleType type)
        : startTicks{startTicks}, endTicks{endTicks}, type{type} {}

};

struct TimeDist {
    uint64_t min;
    uint64_t avg;
    uint64_t p50;
    uint64_t p90;
    uint64_t p99;
    uint64_t p999;
    uint64_t p9999;
    uint64_t p99999;
    uint64_t throughput;
};


class YCSBWorkload {
public:
    YCSBWorkload(Client *client, uint32_t readPercent, uint64_t targetOps, uint32_t objectCount,
                 uint32_t objectSize = 128) :
        client(client), readPercent(readPercent), targetOps(targetOps), objectSize(objectSize)
        , samples(), zipfianGenerator(), experimentStartTime(0) {
        zipfianGenerator = new ZipfianGenerator(objectCount);
    }

    void run(uint64_t seconds) {
        char value[objectSize];
        Buffer buffer;
        uint32_t choice = std::rand() % 100;
        const uint64_t oneSecond = Cycles::fromSeconds(1);

        experimentStartTime = Cycles::rdtsc();

        uint64_t opCount = 0;
        uint64_t targetNSPO = 0;
        if (targetOps > 0) {
            targetNSPO = 1000000000 / targetOps;
            // Randomize start time
            Cycles::sleep(static_cast<uint64_t>((rand() % targetNSPO) / 1000));
        }
        while (true) {
            uint64_t start = Cycles::rdtsc();
            SampleType type;
            uint64_t key = zipfianGenerator->nextNumber();
            if (choice < readPercent) {
                type = GET;
                client->get(key, &buffer);
            } else {
                type = PUT;
                client->put(key, value, objectSize);
            }

            uint64_t stop = Cycles::rdtsc();

            samples.emplace_back(start, stop, type);

            if (experimentStartTime + seconds * oneSecond < stop)
                break;

            if (targetNSPO > 0) {
                uint64_t nextStop = experimentStartTime +
                                    Cycles::fromNanoseconds(
                                        (opCount * targetNSPO) +
                                        (rand() % targetNSPO) -
                                        (targetNSPO / 2));
                while (Cycles::rdtsc() < nextStop);
            }
        }
        std::vector<TimeDist> result;
        statistics(result);

        printf("time: median, 99th, average, throughput\n");
        for (uint64_t i = 0; i < result.size(); i++) {
            printf(
                "%lu:%lu, %lu, %lu, %lf\n",
                i, result[i].p50, result[i].p999, result[i].avg,
                static_cast<double>(result[i].throughput) / 100.);
        }
    }

    static void getDist(std::vector<uint64_t> &times, TimeDist *dist) {
        int count = static_cast<int>(times.size());
        std::sort(times.begin(), times.end());
        dist->avg = 0;
        dist->min = 0;
        uint64_t last = 0;
        uint64_t sum = 0;

        for (uint64_t time: times) {
            sum += Cycles::toMicroseconds(time);
        }

        if (count > 0) {
            dist->avg = sum / count;
            dist->min = Cycles::toMicroseconds(times[0]);
            last = times.back();
        }


        dist->throughput = times.size();
        int index = count / 2;
        if (index < count) {
            dist->p50 = Cycles::toMicroseconds(times.at(index));
        } else {
            dist->p50 = last;
        }
        index = count - (count + 5) / 10;
        if (index < count) {
            dist->p90 = Cycles::toMicroseconds(times.at(index));
        } else {
            dist->p90 = last;
        }
        index = count - (count + 50) / 100;
        if (index < count) {
            dist->p99 = Cycles::toMicroseconds(times.at(index));
        } else {
            dist->p99 = last;
        }
        index = count - (count + 500) / 1000;
        if (index < count) {
            dist->p999 = Cycles::toMicroseconds(times.at(index));
        } else {
            dist->p999 = last;
        }
        index = count - (count + 5000) / 10000;
        if (index < count) {
            dist->p9999 = Cycles::toMicroseconds(times.at(index));
        } else {
            dist->p9999 = last;
        }
        index = count - (count + 50000) / 100000;
        if (index < count) {
            dist->p99999 = Cycles::toMicroseconds(times.at(index));
        } else {
            dist->p99999 = last;
        }
    }

    void statistics(std::vector<TimeDist> &result) {
        std::map<uint64_t, std::vector<uint64_t> *> latency;
        printf("%lu\n", samples.size());
        for (Sample &sample: samples) {
            uint64_t timestamp = Cycles::toMicroseconds(
                sample.startTicks - experimentStartTime) / 1000 / 100;
            if (latency.find(timestamp) == latency.end()) {
                for (uint64_t i = latency.size(); i <= timestamp; i++)
                    latency[i] = new std::vector<uint64_t>();
            }
            latency[timestamp]->push_back(
                sample.endTicks - sample.startTicks);
        }

        for (uint64_t i = 0; i < latency.size(); i++) {
            result.emplace_back();
            getDist(*latency[i], &result.back());
        }
    }

private:
    Client *client;
    uint32_t readPercent;
    uint64_t targetOps;
    uint32_t objectSize;

    std::vector<Sample> samples;

    ZipfianGenerator *zipfianGenerator;

    uint64_t experimentStartTime;
};

int main(int argc, char *argv[]) {

    OptionConfig optionConfig;
    optionConfig.parse(argc, argv);

    Logger::log("client connect to %s", optionConfig.connectLocator.c_str());

    Context context(optionConfig, true);

    Client client(&context, optionConfig.connectLocator);


    YCSBWorkload workload(&client, optionConfig.readPercent, optionConfig.targetOps, optionConfig.objectCount,
                          optionConfig.objectSize);

    workload.run(optionConfig.time);
    Logger::log("Benchmark finished");
}
