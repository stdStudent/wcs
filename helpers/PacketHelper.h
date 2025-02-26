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

class PacketHelper {
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

    std::vector<BYTE> parseHexStringToBytes(const std::string& hexStr) {
        std::vector<BYTE> bytes;
        std::istringstream iss(hexStr);
        std::string token;
        iss.ignore(2); // Skip "[ "

        while (iss >> token) {
            if (!token.empty() && token.back() == ']') token.pop_back();
            if (token.size() < 2 || token.substr(0, 2) != "0x") continue;

            unsigned int byteValue;
            std::stringstream converter;
            converter << std::hex << token.substr(2);
            converter >> byteValue;
            bytes.push_back(static_cast<BYTE>(byteValue));
        }

        return bytes;
    }

    CryptHelper& cryptHelper;

public:
    class Server {
    public:
        Server(PacketHelper& parent) : parent(parent) {}

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
        PacketHelper& parent;
    };

    class Client {
    public:
        Client(PacketHelper& parent) : parent(parent) {}

        std::string getPacketList() {
            const std::string uuid = parent.generateUUID();
            return parent.buildClientPacket("list", uuid, "");
        }

        std::string getPacketGet(const std::string& fileName) {
            const std::string uuid = parent.generateUUID();
            return parent.buildClientPacket("get", uuid, fileName);
        }

    private:
        PacketHelper& parent;
    };

    class ServerPacket {
    private:
        std::string id_;
        std::string uuid_;
        size_t totalBytes_ = 0;
        size_t amountOfPackets_ = 0;
        size_t packetNumber_ = 0;
        size_t contentBytes_ = 0;
        std::vector<BYTE> contentChecksum_;
        std::vector<BYTE> content_;

    public:
        const std::string& getId() const { return id_; }
        const std::string& getUuid() const { return uuid_; }
        const size_t getTotalBytes() const { return totalBytes_; }
        const size_t getAmountOfPackets() const { return amountOfPackets_; }
        const size_t getPacketNumber() const { return packetNumber_; }
        const size_t getContentBytes() const { return contentBytes_; }
        const std::vector<BYTE>& getContentChecksum() const { return contentChecksum_; }
        const std::vector<BYTE>& getContent() const { return content_; }

        void setId(const std::string& id) { id_ = id; }
        void setUuid(const std::string& uuid) { uuid_ = uuid; }
        void setTotalBytes(size_t totalBytes) { totalBytes_ = totalBytes; }
        void setAmountOfPackets(size_t amount) { amountOfPackets_ = amount; }
        void setPacketNumber(size_t number) { packetNumber_ = number; }
        void setContentBytes(size_t bytes) { contentBytes_ = bytes; }
        void setContentChecksum(const std::vector<BYTE>& checksum) { contentChecksum_ = checksum; }
        void setContent(const std::vector<BYTE>& content) { content_ = content; }
    };

    // Client-specific parsed packet
    class ClientPacket {
    private:
        std::string id_;
        std::string uuid_;
        std::string argument_;

    public:
        const std::string& getId() const { return id_; }
        const std::string& getUuid() const { return uuid_; }
        const std::string& getArgument() const { return argument_; }

        void setId(const std::string& id) { id_ = id; }
        void setUuid(const std::string& uuid) { uuid_ = uuid; }
        void setArgument(const std::string& argument) { argument_ = argument; }
    };

    // Parser for server packets
    ServerPacket parseServerPacket(const std::string& packetStr) {
        ServerPacket parsedPacket;
        std::istringstream iss(packetStr);
        std::string line;

        if (!std::getline(iss, line) || line != "START_PACKET") {
            return parsedPacket;
        }

        while (std::getline(iss, line)) {
            if (line == "END_PACKET") break;
            const size_t colonPos = line.find(": ");
            if (colonPos == std::string::npos) continue;

            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 2);

            if (key == "ID") parsedPacket.setId(value);
            else if (key == "UUID") parsedPacket.setUuid(value);
            else if (key == "TOTAL_BYTES") parsedPacket.setTotalBytes(std::stoul(value));
            else if (key == "AMOUNT_OF_PACKETS") parsedPacket.setAmountOfPackets(std::stoul(value));
            else if (key == "PACKET_NUMBER") parsedPacket.setPacketNumber(std::stoul(value));
            else if (key == "CONTENT_BYTES") parsedPacket.setContentBytes(std::stoul(value));
            else if (key == "CONTENT_CHECKSUM") parsedPacket.setContentChecksum(parseHexStringToBytes(value));
            else if (key == "CONTENT") parsedPacket.setContent(parseHexStringToBytes(value));
        }

        return parsedPacket;
    }

    // Parser for client packets
    ClientPacket parseClientPacket(const std::string& packetStr) {
        ClientPacket parsedPacket;
        std::istringstream iss(packetStr);
        std::string line;

        if (!std::getline(iss, line) || line != "START_PACKET") {
            return parsedPacket;
        }

        while (std::getline(iss, line)) {
            if (line == "END_PACKET") break;
            size_t colonPos = line.find(": ");
            if (colonPos == std::string::npos) continue;

            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 2);

            if (key == "ID") parsedPacket.setId(value);
            else if (key == "UUID") parsedPacket.setUuid(value);
            else if (key == "ARGUMENT") parsedPacket.setArgument(value);
        }

        return parsedPacket;
    }

    PacketHelper(CryptHelper& cryptHelper) :
        cryptHelper(cryptHelper),
        server(*this),
        client(*this) {}

    Server server;
    Client client;
};

#endif //PACKETBUILDHELPER_H