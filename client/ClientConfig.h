#ifndef CLIENTCONFIG_H
#define CLIENTCONFIG_H

#include "ConfigHelper.h"
#include "FileHelper.h"

class ClientConfig {

public:
    std::string serverIp;
    short serverPort;
    std::string filesDir;

    ClientConfig(ConfigHelper& config) {
        this->serverIp = config.readIni("Server", "ip");

        const auto serverPort = config.readIni("Server", "port");
        this->serverPort = static_cast<short>(std::stoi(serverPort));

        this->filesDir = config.readIni("Files", "dir");
        FileHelper::createAllSubdirectories(filesDir);
    }

    std::string toString() const {
        std::string result;
        result += "serverIp: " + std::string(serverIp) + "\n";
        result += "serverPort: " + std::to_string(serverPort) + "\n";
        result += "filesDir: " + filesDir + "\n";
        return result;
    }
};

#endif //CLIENTCONFIG_H
