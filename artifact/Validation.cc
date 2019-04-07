#include <OptionConfig.h>
#include <Logger.h>
#include <Context.h>
#include <Client.h>

using namespace Gungnir;

const std::string DOESNT_EXISTS = "DOESN'T EXISTS";

void put(Client &client, uint64_t key, const std::string &value) {
    client.put(key, value.c_str(), value.length());
}

std::string get(Client &client, uint64_t key) {
    bool exists;
    Buffer buffer;
    client.get(key, &buffer, &exists);
    if (!exists)
        return DOESNT_EXISTS;
    return std::string(static_cast<char *>( buffer.getRange(0, buffer.size())), buffer.size());
}

void erase(Client &client, uint64_t key) {
    client.erase(key);
}

int main(int argc, char *argv[]) {
    OptionConfig optionConfig;
    optionConfig.parse(argc, argv);
    Logger::log("client connect to %s", optionConfig.connectLocator.c_str());
    Context context(optionConfig, true);
    Client client(&context, optionConfig.connectLocator);
    for (int i = 0; i < 20; i++) {
        put(client, i, "12");
    }
    assert(get(client, 12) == "12");
    put(client, 200, "123");
    std::string res = get(client, 200);
    assert(res == "123");
    erase(client, 7);
    assert(get(client, 7) == DOESNT_EXISTS);
}


