#include "PeerBridgeManager.h"
#include <iostream>

int main() {
    int port = 8080;
    PeerBridgeManager server(port);

    std::cout << "Starting signaling server..." << std::endl;
    server.startServer();

    return 0;
}

