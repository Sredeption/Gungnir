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
    for (int i = 0; i < 200; i++) {
        put(client, i, "12");
    }
    for (int i = 1000; i < 20000; i++) {
        put(client, i, std::to_string(i));
    }
    assert(get(client, 12) == "12");
    put(client, 200, "123");
    std::string res = get(client, 200);
    assert(res == "123");
    erase(client, 7);
    assert(get(client, 7) == DOESNT_EXISTS);
    Iterator iterator = client.scan(2000, 5000);
    uint64_t key = 2000;

    for (; !iterator.isDone(); iterator.next()) {
        uint64_t actual = iterator.getKey();
        uint32_t len;
        char *content = static_cast<char *>(iterator.getValue(len));
        assert(key == actual);
        std::string value = std::string(content, len);
        assert(value == std::to_string(key));
        key++;
    }

    assert(key==5001);

    Logger::log("validation finished");
}


