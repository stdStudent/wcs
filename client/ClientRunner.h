#ifndef CLIENTRUNNER_H
#define CLIENTRUNNER_H

#include <winsock2.h>
#include <windows.h>
#include <string>
#include <queue>
#include <vector>
#include <ws2tcpip.h>

class ClientRunner {
private:
    SOCKET m_socket;
    std::queue<std::string> m_pendingCommands;
    std::vector<std::string> m_receivedResponses;
    bool m_isConnected;

    // Buffer for accumulating received data
    std::string m_receiveBuffer;

    // Constants for packet markers
    const std::string START_MARKER = "START_PACKET";
    const std::string END_MARKER = "END_PACKET";

public:
    ClientRunner() : m_socket(INVALID_SOCKET), m_isConnected(false) {
        // Initialize Winsock
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    ~ClientRunner() {
        disconnect();
    }

    bool connectToServer(const std::string& serverIP, const unsigned short serverPort) {
        // Create socket
        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET) return false;

        // Set non-blocking mode
        u_long mode = 1;
        ioctlsocket(m_socket, FIONBIO, &mode);

        // Connect to server
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
        serverAddr.sin_port = htons(serverPort);

        connect(m_socket, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr));

        // Since socket is non-blocking, connect returns immediately
        // We need to check connection status
        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(m_socket, &writeSet);

        // Wait for connection with timeout
        constexpr timeval timeout = {5, 0}; // 5 seconds timeout
        if (select(0, nullptr, &writeSet, nullptr, &timeout) <= 0) {
            closesocket(m_socket);
            return false;
        }

        m_isConnected = true;
        return true;
    }

    void disconnect() {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        m_isConnected = false;
        // Clear the receive buffer when disconnecting
        m_receiveBuffer.clear();
        WSACleanup();
    }

    // Queue a command to be sent when possible
    void queueCommand(const std::string& command) {
        m_pendingCommands.push(command);
    }

    // Main update function to call in the application loop
    void update() {
        if (!m_isConnected) return;

        // Check if socket is ready for reading/writing
        fd_set readSet, writeSet;
        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);
        FD_SET(m_socket, &readSet);

        // Only add to write set if we have commands to send
        if (!m_pendingCommands.empty()) {
            FD_SET(m_socket, &writeSet);
        }

        // Zero timeout makes select return immediately
        constexpr timeval timeout = {0, 0};
        const int ready = select(0, &readSet, &writeSet, nullptr, &timeout);

        if (ready > 0) {
            // Process outgoing commands
            if (FD_ISSET(m_socket, &writeSet) && !m_pendingCommands.empty()) {
                sendNextCommand();
            }

            // Process incoming responses
            if (FD_ISSET(m_socket, &readSet)) {
                receiveResponses();
            }
        }
    }

    // Get any responses that have been received
    const std::vector<std::string>& getResponses() {
        return m_receivedResponses;
    }

    // Clear the received responses
    void clearResponses() {
        m_receivedResponses.clear();
    }

private:
    void sendNextCommand() {
        const std::string cmd = m_pendingCommands.front();
        const int result = send(m_socket, cmd.c_str(), cmd.length(), 0);

        if (result != SOCKET_ERROR) {
            // Command sent successfully
            m_pendingCommands.pop();
        } else if (WSAGetLastError() != WSAEWOULDBLOCK) {
            // An error occurred
            disconnect();
        }
    }

    void receiveResponses() {
        char buffer[4096];
        const int bytesReceived = recv(m_socket, buffer, sizeof(buffer) - 1, 0);

        if (bytesReceived > 0) {
            // Append new data to existing buffer
            buffer[bytesReceived] = '\0';
            m_receiveBuffer.append(buffer, bytesReceived);

            // Process all complete packets in the buffer
            processBuffer();
        } else if (bytesReceived == 0 || WSAGetLastError() != WSAEWOULDBLOCK) {
            // Connection closed or error
            disconnect();
        }
    }

    void processBuffer() {
        // Process and extract all complete packets from the buffer
        size_t startPos = 0;

        // Continue as long as we can find a start marker in the remaining buffer
        while ((startPos = m_receiveBuffer.find(START_MARKER, startPos)) != std::string::npos) {
            // Look for an end marker after this start marker
            size_t endPos = m_receiveBuffer.find(END_MARKER, startPos);

            // If we found a complete packet
            if (endPos != std::string::npos) {
                // Extract the complete packet (including END_MARKER which is 10 chars long)
                std::string packet = m_receiveBuffer.substr(
                    startPos,
                    endPos - startPos + END_MARKER.length()
                );

                // Add to our received responses
                m_receivedResponses.push_back(packet);

                // Move start position to just after this packet
                startPos = endPos + END_MARKER.length();
            } else {
                // We found a start but no end - this is a partial packet
                // Leave it in the buffer and exit the loop to wait for more data
                break;
            }
        }

        // Remove all processed data from the buffer
        // If startPos is 0, no complete packets were found, so keep everything
        // If startPos > 0, we've processed up to startPos, so remove that portion
        if (startPos > 0) {
            m_receiveBuffer.erase(0, startPos);
        }

        constexpr size_t MAX_BUFFER_SIZE = 10 * 1024 * 1024; // 10MB max buffer
        if (m_receiveBuffer.size() > MAX_BUFFER_SIZE) {
            m_receiveBuffer.clear();
        }
    }
};

#endif //CLIENTRUNNER_H