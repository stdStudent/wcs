#ifndef PACKETBUILDHELPER_H
#define PACKETBUILDHELPER_H

#include <fstream>
#include <queue>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <random>
#include <filesystem>

#include "CryptHelper.h"

class PacketBuildHelper {
private:
    std::string generateUUID() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);

        std::stringstream ss;
        for (int i = 0; i < 32; ++i)
            ss << std::hex << dis(gen);

        return ss.str();
    }

    std::string bytesToHexString(const std::vector<BYTE>& bytes) {
        std::stringstream ss;

        ss << "[ ";
        for (const BYTE b : bytes)
            ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
        ss << "]";

        return ss.str();
    }

    std::string buildServerPacket(
        const std::string& id,
        const std::string& uuid,
        const size_t totalBytes,
        const size_t amountOfPackets,
        const size_t packetNumber,
        const size_t contentBytes,
        const std::string& checksumStr,
        const std::string& contentStr) {

        std::stringstream packet;
        packet << "START_PACKET\n"
               << "ID: " << id << "\n"
               << "UUID: " << uuid << "\n"
               << "TOTAL_BYTES: " << totalBytes << "\n"
               << "AMOUNT_OF_PACKETS: " << amountOfPackets << "\n"
               << "PACKET_NUMBER: " << packetNumber << "\n"
               << "CONTENT_BYTES: " << contentBytes << "\n"
               << "CONTENT_CHECKSUM: " << checksumStr << "\n"
               << "CONTENT: " << contentStr << "\n"
               << "END_PACKET";

        return packet.str();
    }

    std::string buildClientPacket(
        const std::string& id,
        const std::string& uuid,
        const std::string& argument) {

        std::stringstream packet;
        packet << "START_PACKET\n"
               << "ID: " << id << "\n"
               << "UUID: " << uuid << "\n"
               << "ARGUMENT: " << argument << "\n"
               << "END_PACKET";

        return packet.str();
    }

    CryptHelper& cryptHelper;

public:
    class Server {
    public:
        Server(PacketBuildHelper& parent) : parent(parent) {}

        std::queue<std::string> getPacketList(const std::string& pathToDir) {
            std::queue<std::string> packets;
            const std::string uuid = parent.generateUUID();
            size_t totalBytes = 0;
            std::vector<std::string> filenames;

            for (const auto& entry : std::filesystem::directory_iterator(pathToDir)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    filenames.push_back(filename);
                    totalBytes += filename.size();
                }
            }

            const size_t amountOfPackets = filenames.size();

            for (size_t i = 0; i < filenames.size(); ++i) {
                const std::string& filename = filenames[i];
                const std::vector<BYTE> content(filename.begin(), filename.end());
                std::vector<BYTE> checksum = parent.cryptHelper.createHash(content);
                std::string checksumStr = parent.bytesToHexString(checksum);
                std::string contentStr = parent.bytesToHexString(content);

                std::string packetStr = parent.buildServerPacket(
                    "list", uuid, totalBytes, amountOfPackets, i + 1,
                    filename.size(), checksumStr, contentStr);

                packets.push(packetStr);
            }
            return packets;
        }

        std::queue<std::string> getPacketGet(std::fstream& file) {
            std::queue<std::string> packets;
            const std::string uuid = parent.generateUUID();
            file.seekg(0, std::ios::end);
            const auto totalBytes = static_cast<size_t>(file.tellg());
            file.seekg(0, std::ios::beg);

            constexpr size_t chunkSize = 1024;
            const size_t amountOfPackets = (totalBytes + chunkSize - 1) / chunkSize;

            for (size_t packetNumber = 1; packetNumber <= amountOfPackets; ++packetNumber) {
                std::vector<BYTE> chunk(chunkSize);
                file.read(reinterpret_cast<char*>(chunk.data()), chunkSize);
                const size_t bytesRead = static_cast<size_t>(file.gcount());
                if (bytesRead < chunkSize) {
                    chunk.resize(bytesRead);
                }

                std::vector<BYTE> checksum = parent.cryptHelper.createHash(chunk);
                std::string checksumStr = parent.bytesToHexString(checksum);
                std::string contentStr = parent.bytesToHexString(chunk);

                std::string packetStr = parent.buildServerPacket(
                    "get", uuid, totalBytes, amountOfPackets, packetNumber,
                    bytesRead, checksumStr, contentStr);

                packets.push(packetStr);
            }
            return packets;
        }

    private:
        PacketBuildHelper& parent;
    };

    class Client {
    public:
        Client(PacketBuildHelper& parent) : parent(parent) {}

        std::string getPacketList() {
            const std::string uuid = parent.generateUUID();
            return parent.buildClientPacket("list", uuid, "");
        }

        std::string getPacketGet(const std::string& fileName) {
            const std::string uuid = parent.generateUUID();
            return parent.buildClientPacket("get", uuid, fileName);
        }

    private:
        PacketBuildHelper& parent;
    };

    PacketBuildHelper(CryptHelper& cryptHelper) :
        cryptHelper(cryptHelper),
        server(*this),
        client(*this) {}

    Server server;
    Client client;
};

#endif //PACKETBUILDHELPER_H