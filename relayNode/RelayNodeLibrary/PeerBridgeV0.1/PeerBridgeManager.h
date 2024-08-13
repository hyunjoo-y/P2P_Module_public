// PeerBridgeManager.h
#ifndef PEERBRIDGEMANAGER_H
#define PEERBRIDGEMANAGER_H

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class PeerBridgeManager {
public:
    PeerBridgeManager(int port);
    ~PeerBridgeManager();
    void startServer();

private:
    int port;
    int server_fd;
    std::map<std::string, int> clientSockets; // Map peer ID to client socket
    std::mutex clientMutex; // Mutex to protect clientSockets map

    void handleClient(int clientSocket);
    void sendMessageToPeer(const std::string &nodeAddr, const std::string &message);
    json receiveJsonFromPeer(int clientSocket);
    void processIceCandidate(const std::string &nodeAddr, const json &candidateJson);
};

#endif // PEERBRIDGEMANAGER_H

