#include <iostream>

#include "ClientConfig.h"
#include "ConfigHelper.h"

int main() {
    auto config = ConfigHelper("client.ini");
    auto clientConfig = ClientConfig(config);
    std::cout << clientConfig.toString() << std::endl;

    return 0;
}
