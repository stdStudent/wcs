#ifndef MESSAGEPROCESSOR_H
#define MESSAGEPROCESSOR_H

#include <functional>
#include <iostream>
#include <queue>
#include <string>

#include "PacketHelper.h"
#include "ServerConfig.h"
#include "ServerRunner.h"

class MessageProcessor {
private:
    ServerConfig& serverConfig;
    PacketHelper& packetHelper;

public:
    MessageProcessor(ServerConfig& serverConfig, PacketHelper& packetHelper)
        : serverConfig(serverConfig), packetHelper(packetHelper) {}

    MessageHandler messageHandler = [&](const std::string& message, SOCKET clientSocket) {
        std::cout << "Received: \n" << message << " | From client: " << clientSocket << std::endl;

        const auto clientPacket = packetHelper.parseClientPacket(message);
        const auto& clientPacketUUID = clientPacket.getUuid();

        std::queue<std::string> serverPackets;
        if (clientPacket.getId() == "list") {
            const auto dir = serverConfig.filesDir;
            serverPackets = packetHelper.server.getPacketList(clientPacketUUID, dir);
        } else if (clientPacket.getId() == "get") {
            const auto& fileName = clientPacket.getArgument();
            std::string filePath = serverConfig.filesDir + "\\" + fileName;
            std::fstream file(filePath, std::ios::in | std::ios::binary);
            serverPackets = packetHelper.server.getPacketGet(clientPacketUUID, fileName, file);
        }

        return serverPackets;
    };

};

#endif //MESSAGEPROCESSOR_H
