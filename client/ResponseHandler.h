#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#include "PacketHelper.h"

class ResponseHandler {
private:
    ClientConfig& clientConfig;
    PacketHelper& packageHelper;

    std::unordered_map<std::string, std::vector<std::string>> listResponseMap;

public:
    ResponseHandler(ClientConfig& clientConfig, PacketHelper& packageHelper)
        : clientConfig(clientConfig), packageHelper(packageHelper) {}

    void handleResponses(const std::vector<std::string> responses) {
        for (const auto& response : responses) {
            const auto serverPacket = packageHelper.parseServerPacket(response);
            const auto& serverPacketId = serverPacket.getId();
            const auto& uuid = serverPacket.getUuid();
            const auto packetNumber = serverPacket.getPacketNumber();
            const auto amountOfPackets = serverPacket.getAmountOfPackets();

            // if packet id if get, write file chunk
            if (serverPacketId == "get") {
                const auto& fileName = serverPacket.getArgument();
                const auto& filePath = clientConfig.filesDir + "\\" + fileName;

                const auto& content = serverPacket.getContent();
                std::ofstream file(filePath, std::ios::app | std::ios::binary);

                file.write(reinterpret_cast<const char*>(content.data()), content.size());
                file.close();
            } else
                listResponseMap[uuid].push_back(response);

            if (packetNumber == amountOfPackets) {
                if (serverPacketId == "list") {
                    std::string content;
                    for (const auto& packet : listResponseMap[uuid]) {
                        const auto& packetContent = packageHelper.parseServerPacket(packet).getContent();
                        for (const auto& chunk : packetContent)
                            content += chunk;
                        content += '\n';
                    }

                    if (content.empty()) {
                        std::cout << "No files found" << std::endl;
                    } else {
                        std::cout << std::string(content.begin(), content.end()) << '\n' << std::endl;
                    }

                    listResponseMap.erase(uuid);
                } else if (serverPacketId == "get") {
                    const auto& fileName = serverPacket.getArgument();
                    std::cout << "File: " << fileName << " has been saved." << '\n' << std::endl;
                }
            }
        }
    }

};

#endif //COMMANDHANDLER_H
