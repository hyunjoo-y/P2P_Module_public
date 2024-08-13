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
            if (j.contains("type")) {
                std::string messageType = j["type"];
                if (messageType == "register") {
                    std::string nodeAddr = j["nodeAddr"]; 
                    std::lock_guard<std::mutex> lock(clientMutex);
                    if (clientSockets.find(nodeAddr) != clientSockets.end()) {
                        std::cout << "Already registered peer: " << nodeAddr << std::endl;
                    } else {
                        clientSockets[nodeAddr] = clientSocket;
                        std::cout << "Registered peer: " << nodeAddr << std::endl;
                    }
                }
                else if (messageType == "iceCandidates") {
                    std::string targetNodeAddr = j["nodeAddr"];
                    if (clientSockets.find(targetNodeAddr) != clientSockets.end()) {
                        sendMessageToPeer(targetNodeAddr, j.dump());
                    } else {
                        std::cout << "Target peer not found: " << targetNodeAddr << std::endl;
                        // 대기 로직 추가
                        // std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 예시: 1초 대기
                    }
                }else if (messageType == "checkConnection") {
                    std::string targetNodeAddr = j["targetNodeAddr"];
                    std::cout << "Check peer: " << targetNodeAddr << std::endl;
                    bool connected = (clientSockets.find(targetNodeAddr) != clientSockets.end());
                    json response;
                    response["connected"] = connected;
                    std::string serializedResponse = response.dump() + "\n";
                    send(clientSocket, serializedResponse.c_str(), serializedResponse.size(), 0);
                }
            }
        } catch (std::exception& e) {
            std::cerr << "Error handling client: " << e.what() << std::endl;
            break;  // 에러 발생 시 루프 종료
        }
    }
    close(clientSocket);  // 작업이 완료되면 클라이언트 소켓 닫기
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

