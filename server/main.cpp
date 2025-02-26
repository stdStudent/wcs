#include <iostream>

#include "ConfigHelper.h"
#include "ServerConfig.h"

int main() {
    ConfigHelper config("server.ini");
    ServerConfig serverConfig(config);
    std::cout << serverConfig.toString() << std::endl;

    return 0;
}
