#ifndef SERVERRUNNER_H
#define SERVERRUNNER_H

#pragma once

#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <ranges>
#include <string>
#include <queue>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>
#include <functional>
#include <unordered_map>

class ServerRunner {
public:
    // Define constants for buffer sizes and thread pool size
    static constexpr size_t DEFAULT_BUFFER_SIZE = 4096;
    static constexpr int DEFAULT_THREAD_COUNT = 2;
    static constexpr int DEFAULT_PORT = 8080;

    // Callback function type for processing received messages
    using MessageHandler = std::function<std::queue<std::string>(const std::string&, SOCKET)>;

    // Per-connection data structure
    struct ConnectionContext {
        SOCKET socket;                   // Client socket
        WSAOVERLAPPED recvOverlapped;    // Overlapped structure for recv operations
        WSAOVERLAPPED sendOverlapped;    // Overlapped structure for send operations
        char recvBuffer[DEFAULT_BUFFER_SIZE]; // Buffer for receiving data
        std::vector<char> sendBuffer;    // Buffer for sending data
        WSABUF wsaRecvBuffer;            // WSA buffer for recv operations
        WSABUF wsaSendBuffer;            // WSA buffer for send operations
        std::queue<std::string> pendingSends; // Queue of messages waiting to be sent
        std::mutex sendMutex;            // Mutex to protect pendingSends
        bool isSending;                  // Flag to indicate if a send operation is in progress

        ConnectionContext(const SOCKET s) : socket(s), isSending(false) {
            ZeroMemory(&recvOverlapped, sizeof(WSAOVERLAPPED));
            ZeroMemory(&sendOverlapped, sizeof(WSAOVERLAPPED));
            ZeroMemory(recvBuffer, DEFAULT_BUFFER_SIZE);

            wsaRecvBuffer.buf = recvBuffer;
            wsaRecvBuffer.len = DEFAULT_BUFFER_SIZE;

            wsaSendBuffer.buf = nullptr;
            wsaSendBuffer.len = 0;
        }
    };

    // Constructor
    ServerRunner(const unsigned short port = DEFAULT_PORT, const size_t threadCount = DEFAULT_THREAD_COUNT)
        : m_port(port), m_threadCount(threadCount), m_running(false), m_completionPort(nullptr), m_listenSocket(INVALID_SOCKET) {
    }

    // Destructor
    ~ServerRunner() {
        stop();
    }

    // Start the server
    bool start(const MessageHandler &handler) {
        m_messageHandler = handler;

        // Initialize Winsock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            printf("WSAStartup failed: %d\n", result);
            return false;
        }

        // Create completion port
        m_completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if (m_completionPort == nullptr) {
            printf("CreateIoCompletionPort failed: %d\n", GetLastError());
            WSACleanup();
            return false;
        }

        // Create listening socket
        m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSocket == INVALID_SOCKET) {
            printf("socket failed: %d\n", WSAGetLastError());
            CloseHandle(m_completionPort);
            WSACleanup();
            return false;
        }

        // Set up server address
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(m_port);

        // Bind socket
        result = bind(m_listenSocket, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr));
        if (result == SOCKET_ERROR) {
            printf("bind failed: %d\n", WSAGetLastError());
            closesocket(m_listenSocket);
            CloseHandle(m_completionPort);
            WSACleanup();
            return false;
        }

        // Start listening
        result = listen(m_listenSocket, SOMAXCONN);
        if (result == SOCKET_ERROR) {
            printf("listen failed: %d\n", WSAGetLastError());
            closesocket(m_listenSocket);
            CloseHandle(m_completionPort);
            WSACleanup();
            return false;
        }

        // Associate the listen socket with the completion port
        if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_listenSocket), m_completionPort, 0, 0) == nullptr) {
            printf("CreateIoCompletionPort for listen socket failed: %d\n", GetLastError());
            closesocket(m_listenSocket);
            CloseHandle(m_completionPort);
            WSACleanup();
            return false;
        }

        m_running = true;

        // Start the accept thread
        m_acceptThread = CreateThread(
            nullptr,                         // Default security attributes
            0,                               // Default stack size
            &ServerRunner::acceptThreadFunc, // Thread function
            this,                            // Parameter to thread function
            0,                               // Run immediately
            &m_acceptThreadId                // Thread identifier
        );

        if (m_acceptThread == nullptr) {
            printf("Failed to create accept thread: %d\n", GetLastError());
            stop();
            return false;
        }

        // Start worker threads
        m_workerThreads.resize(m_threadCount);
        m_workerThreadIds.resize(m_threadCount);

        for (size_t i = 0; i < m_threadCount; i++) {
            m_workerThreads[i] = CreateThread(
                nullptr,                          // Default security attributes
                0,                                // Default stack size
                &ServerRunner::workerThreadFunc,  // Thread function
                this,                             // Parameter to thread function
                0,                                // Run immediately
                &m_workerThreadIds[i]             // Thread identifier
            );

            if (m_workerThreads[i] == nullptr) {
                printf("Failed to create worker thread %zu: %d\n", i, GetLastError());
                stop();
                return false;
            }
        }

        printf("Server started on port %d with %llu worker threads\n", m_port, m_threadCount);
        return true;
    }

    // Stop the server
    void stop() {
        if (!m_running) return;

        m_running = false;

        // Close the listen socket to stop accepting new connections
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;

        // Post completion packets to wake up worker threads
        for (size_t i = 0; i < m_workerThreads.size(); i++) {
            PostQueuedCompletionStatus(m_completionPort, 0, static_cast<ULONG_PTR>(NULL), nullptr);
        }

        // Wait for accept thread to finish
        if (m_acceptThread != nullptr) {
            WaitForSingleObject(m_acceptThread, INFINITE);
            CloseHandle(m_acceptThread);
            m_acceptThread = nullptr;
        }

        // Wait for worker threads to finish
        if (!m_workerThreads.empty()) {
            WaitForMultipleObjects(
                m_workerThreads.size(),
                m_workerThreads.data(),
                TRUE,
                INFINITE
            );

            // Close thread handles
            for (const auto& thread : m_workerThreads) {
                CloseHandle(thread);
            }
            m_workerThreads.clear();
            m_workerThreadIds.clear();
        }

        // Close all client connections
        for (const auto& connectionContext: m_connections | std::views::values) {
            closesocket(connectionContext->socket);
            delete connectionContext;
        }
        m_connections.clear();

        // Clean up completion port
        if (m_completionPort != nullptr) {
            CloseHandle(m_completionPort);
            m_completionPort = nullptr;
        }

        // Clean up Winsock
        WSACleanup();

        printf("Server stopped\n");
    }

private:
    // Static thread entry point functions
    static DWORD WINAPI acceptThreadFunc(LPVOID lpParam) {
        auto* server = static_cast<ServerRunner*>(lpParam);
        server->acceptThreadProc();
        return 0;
    }

    static DWORD WINAPI workerThreadFunc(LPVOID lpParam) {
        auto* server = static_cast<ServerRunner*>(lpParam);
        server->workerThreadProc();
        return 0;
    }

    // Thread procedure for accepting connections
    void acceptThreadProc() {
        while (m_running) {
            // Accept a new connection
            sockaddr_in clientAddr{};
            int addrLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(m_listenSocket, reinterpret_cast<sockaddr *>(&clientAddr), &addrLen);

            if (clientSocket == INVALID_SOCKET) {
                if (m_running) {
                    printf("accept failed: %d\n", WSAGetLastError());
                }
                continue;
            }

            // Get client IP address and port for logging
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
            const int clientPort = ntohs(clientAddr.sin_port);
            printf("New connection from %s:%d\n", clientIP, clientPort);

            // Create a new connection context
            auto* context = new ConnectionContext(clientSocket);

            // Associate the client socket with the completion port
            if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(clientSocket), m_completionPort, reinterpret_cast<ULONG_PTR>(context), 0) == nullptr) {
                printf("CreateIoCompletionPort for client socket failed: %d\n", GetLastError());
                delete context;
                closesocket(clientSocket);
                continue;
            }

            // Store the connection context
            {
                std::lock_guard lock(m_connectionsMutex);
                m_connections[clientSocket] = context;
            }

            // Start receiving data from the client
            postRecv(context);
        }
    }

    // Thread procedure for worker threads
    void workerThreadProc() {
        while (m_running) {
            DWORD bytesTransferred = 0;
            ConnectionContext* context = nullptr;
            LPOVERLAPPED overlapped = nullptr;

            // Wait for a completion packet
            const BOOL result = GetQueuedCompletionStatus(
                m_completionPort,
                &bytesTransferred,
                reinterpret_cast<PULONG_PTR>(&context),
                &overlapped,
                INFINITE
            );

            // Check if the server is shutting down
            if (!m_running) {
                break;
            }

            // Check for errors or client disconnection
            if (!result || (result && bytesTransferred == 0)) {
                if (context) {
                    handleDisconnect(context);
                }
                continue;
            }

            // Process the completion packet
            if (context) {
                if (overlapped == &context->recvOverlapped) {
                    // Handle received data
                    handleRecv(context, bytesTransferred);
                }
                else if (overlapped == &context->sendOverlapped) {
                    // Handle sent data
                    handleSend(context, bytesTransferred);
                }
            }
        }
    }

    // Post a receive operation
    void postRecv(ConnectionContext* context) {
        DWORD flags = 0;
        DWORD bytesRecvd = 0;

        // Reset the recv buffer
        ZeroMemory(context->recvBuffer, DEFAULT_BUFFER_SIZE);
        ZeroMemory(&context->recvOverlapped, sizeof(WSAOVERLAPPED));

        // Post the receive operation
        const int result = WSARecv(
            context->socket,
            &context->wsaRecvBuffer,
            1,
            &bytesRecvd,
            &flags,
            &context->recvOverlapped,
            nullptr
        );

        if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            printf("WSARecv failed: %d\n", WSAGetLastError());
            handleDisconnect(context);
        }
    }

    // Post a send operation
    void postSend(ConnectionContext* context) {
        std::lock_guard lock(context->sendMutex);

        // Check if there's a send operation in progress or no data to send
        if (context->isSending || context->pendingSends.empty()) {
            return;
        }

        // Get the next message to send
        const std::string message = context->pendingSends.front();
        context->pendingSends.pop();

        // Update the send buffer
        context->sendBuffer.resize(message.size());
        memcpy(context->sendBuffer.data(), message.c_str(), message.size());

        // Update the WSA buffer
        context->wsaSendBuffer.buf = context->sendBuffer.data();
        context->wsaSendBuffer.len = static_cast<ULONG>(context->sendBuffer.size());

        // Reset the overlapped structure
        ZeroMemory(&context->sendOverlapped, sizeof(WSAOVERLAPPED));

        // Mark that a send operation is in progress
        context->isSending = true;

        // Post the send operation
        DWORD bytesSent = 0;
        const int result = WSASend(
            context->socket,
            &context->wsaSendBuffer,
            1,
            &bytesSent,
            0,
            &context->sendOverlapped,
            nullptr
        );

        if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            printf("WSASend failed: %d\n", WSAGetLastError());
            handleDisconnect(context);
        }
    }

    // Handle received data
    void handleRecv(ConnectionContext* context, DWORD bytesTransferred) {
        if (bytesTransferred == 0) {
            handleDisconnect(context);
            return;
        }

        // Convert the received data to a string
        const std::string message(context->recvBuffer, bytesTransferred);

        // Process the message with the handler
        auto responses = m_messageHandler(message, context->socket);

        // Queue the responses for sending
        {
            std::lock_guard lock(context->sendMutex);
            while (!responses.empty()) {
                context->pendingSends.push(responses.front());
                responses.pop();
            }
        }

        // Start sending the responses
        postSend(context);

        // Post another receive operation
        postRecv(context);
    }

    // Handle sent data
    void handleSend(ConnectionContext* context, DWORD bytesTransferred) {
        {
            std::lock_guard lock(context->sendMutex);
            context->isSending = false;
        }

        // Send the next message if there are any
        postSend(context);
    }

    // Handle client disconnection
    void handleDisconnect(const ConnectionContext* context) {
        printf("Client disconnected\n");

        // Close the socket
        closesocket(context->socket);

        // Remove the connection from the map
        {
            std::lock_guard lock(m_connectionsMutex);
            m_connections.erase(context->socket);
        }

        // Delete the context
        delete context;
    }

private:
    unsigned short m_port;           // Server port
    size_t m_threadCount;            // Number of worker threads
    std::atomic<bool> m_running;     // Flag to indicate if the server is running
    HANDLE m_completionPort;         // IOCP handle
    SOCKET m_listenSocket;           // Listening socket

    // Windows thread handles and IDs
    HANDLE m_acceptThread;           // Thread for accepting connections
    DWORD m_acceptThreadId;          // Accept thread ID
    std::vector<HANDLE> m_workerThreads; // Worker threads
    std::vector<DWORD> m_workerThreadIds; // Worker thread IDs

    MessageHandler m_messageHandler; // Handler for processing messages

    // Map of active connections
    std::unordered_map<SOCKET, ConnectionContext*> m_connections;
    std::mutex m_connectionsMutex; // Mutex to protect m_connections
};

#endif //SERVERRUNNER_H
