#include <iostream>
#include <windows.h>
#include <conio.h>

#include "ClientConfig.h"
#include "ClientRunner.h"
#include "ConfigHelper.h"
#include "PacketHelper.h"
#include "ResponseHandler.h"

bool isKeyPressed(const int vkCode) {
    if (_kbhit()) {
        const int key = _getch();
        if (key == vkCode)
            return true; // Key pressed
    }
    return false;
}

int main() {
    auto config = ConfigHelper("client.ini");
    auto clientConfig = ClientConfig(config);
    std::cout << clientConfig.toString() << std::endl;

    ClientRunner clientRunner;
    CryptHelper clientCrypter;
    PacketHelper packetHelper(clientCrypter);
    ResponseHandler responseHandler(clientConfig, packetHelper);

    // Connect to server
    if (!clientRunner.connectToServer("127.0.0.1", 8080)) {
        std::cout << "Failed to connect to server!" << std::endl;
        return 1;
    }

    // Queue multiple commands right away without waiting for responses
    clientRunner.queueCommand("init");

    // Main application loop
    while (true) {
        // Update client (process network I/O non-blockingly)
        clientRunner.update();

        // Process any received responses
        const auto& responses = clientRunner.getResponses();
        if (!responses.empty())
            responseHandler.handleResponses(responses);
        clientRunner.clearResponses();

        // if user presses letter 'c', let him type command and press enter (non-blocking)
        auto isPressed = isKeyPressed('c');
        if (isPressed) {
            std::cout << "Enter command: ";
            std::string userInput;

            // Clear the input buffer
            const auto hStdin = GetStdHandle(STD_INPUT_HANDLE);
            FlushConsoleInputBuffer(hStdin);

            std::getline(std::cin, userInput);
            if (userInput == "exit") break;

            std::string command;
            if (userInput == "list") {
                command = packetHelper.client.getPacketList();
            } else if (userInput.find("get") == 0) {
                const auto fileName = userInput.substr(4);
                if (fileName.empty()) {
                    std::cout << "Please provide a filename to get" << std::endl;
                    continue;
                }

                command = packetHelper.client.getPacketGet(fileName);
            }

            if (not command.empty())
                clientRunner.queueCommand(command);
        }

        // Sleep a small amount to prevent CPU spinning
        Sleep(10);
    }

    clientRunner.disconnect();
    return 0;
}
