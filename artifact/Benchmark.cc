#include <Client.h>
#include <Logger.h>

using namespace Gungnir;

int main(int argc, char *argv[]) {

    OptionConfig optionConfig;
    optionConfig.parse(argc, argv);

    Logger::log("client connect to %s", optionConfig.connectLocator.c_str());

    Context context(optionConfig, true);

    Client client(&context, optionConfig.connectLocator);

    Buffer buffer;
    client.get(12, &buffer);
}
