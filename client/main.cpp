#include <iostream>
#include <windows.h>
#include <conio.h>

#include "ClientConfig.h"
#include "ClientRunner.h"
#include "ConfigHelper.h"

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
    const auto clientConfig = ClientConfig(config);
    std::cout << clientConfig.toString() << std::endl;

    ClientRunner clientRunner;

    // Connect to server
    if (!clientRunner.connectToServer("127.0.0.1", 8080)) {
        std::cout << "Failed to connect to server!" << std::endl;
        return 1;
    }

    // Queue multiple commands right away without waiting for responses
    clientRunner.queueCommand("init");

    // Main application loop
    bool running = true;
    while (running) {
        // Update client (process network I/O non-blockingly)
        clientRunner.update();

        // Process any received responses
        const auto& responses = clientRunner.getResponses();
        for (const auto& resp : responses) {
            std::cout << "Received: " << resp << std::endl;
        }
        clientRunner.clearResponses();

        // if user presses letter 'c', let him type command and press enter (non-blocking)
        if (isKeyPressed('c')) {
            std::cout << "Enter command: ";
            std::string command;

            // Clear the input buffer
            const auto hStdin = GetStdHandle(STD_INPUT_HANDLE);
            FlushConsoleInputBuffer(hStdin);

            std::getline(std::cin, command);
            clientRunner.queueCommand(command);

            // Exit if user types "exit"
            if (command == "exit") {
                running = false;
            }
        }

        // Sleep a small amount to prevent CPU spinning
        Sleep(10);
    }

    clientRunner.disconnect();
    return 0;
}
