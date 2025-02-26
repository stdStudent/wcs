#ifndef SERVERCONFIG_H
#define SERVERCONFIG_H

#include "ConfigHelper.h"
#include "FileHelper.h"

class ServerConfig {
public:
    unsigned short serverPort;
    std::string filesDir;

    ServerConfig(ConfigHelper& config) {
        const auto serverPort = config.readIni("Server", "port");
        this->serverPort = static_cast<unsigned short>(std::stoi(serverPort));

        this->filesDir = config.readIni("Files", "dir");
        FileHelper::createAllSubdirectories(filesDir);
    }

    std::string toString() {
        std::string result;
        result += "serverPort: " + std::to_string(serverPort) + "\n";
        result += "filesDir: " + filesDir + "\n";
        return result;
    }
};

#endif //SERVERCONFIG_H
