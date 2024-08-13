// PeerBridgeManager.cpp
#include "PeerBridgeManager.h"
#include <cstring>  // for memset

PeerBridgeManager::PeerBridgeManager(int port) : port(port), server_fd(-1) {}

PeerBridgeManager::~PeerBridgeManager() {
    // Close all client sockets
    for (const auto& client : clientSockets) {
        close(client.second);
    }
    // Close the server socket
    if (server_fd != -1) {
        close(server_fd);
    }
}

void PeerBridgeManager::startServer() {
    sockaddr_in address;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind the socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for client connections
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (true) {
        int new_socket;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
            perror("accept");
            continue;
        }
        std::thread(&PeerBridgeManager::handleClient, this, new_socket).detach();
    }
}

void PeerBridgeManager::handleClient(int clientSocket) {
    while (true) {
        try {
            json j = receiveJsonFromPeer(clientSocket);
            if (j.contains("nodeAddr") && j.contains("candidate")) {
                std::string nodeAddr = j["nodeAddr"];
                json candidate = j["candidate"];
                processIceCandidate(nodeAddr, candidate);
            }
        } catch (std::exception& e) {
            std::cerr << "Error handling client: " << e.what() << std::endl;
            break;  // Exit the loop if there's an error
        }
    }
    close(clientSocket);  // Close the client socket when done
}

void PeerBridgeManager::sendMessageToPeer(const std::string &nodeAddr, const std::string &message) {
    std::lock_guard<std::mutex> lock(clientMutex);
    if (clientSockets.find(nodeAddr) != clientSockets.end()) {
        int peerSocket = clientSockets[nodeAddr];
        send(peerSocket, message.c_str(), message.size(), 0);
    }
}

json PeerBridgeManager::receiveJsonFromPeer(int clientSocket) {
    char buffer[1024] = {0};
    int valread = read(clientSocket, buffer, 1024);
    return json::parse(std::string(buffer, valread));
}

void PeerBridgeManager::processIceCandidate(const std::string &nodeAddr, const json &candidateJson) {
    std::lock_guard<std::mutex> lock(clientMutex);
    if (clientSockets.find(nodeAddr) != clientSockets.end()) {
        int peerSocket = clientSockets[nodeAddr];
        std::string message = candidateJson.dump() + "\n";  // Convert JSON to string and add newline
        send(peerSocket, message.c_str(), message.length(), 0);  // Send the message to the peer
    }
}

