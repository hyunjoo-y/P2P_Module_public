#include "PeerBridgeManager.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

NetworkConnection::NetworkConnection(const std::string& serverAddress, int port)
    : serverAddress_(serverAddress), port_(port), tcpSocket_(-1), udpSocket_(-1) {}

NetworkConnection::~NetworkConnection() {
    if (tcpSocket_ != -1)
        close(tcpSocket_);
    if (udpSocket_ != -1)
        close(udpSocket_);
}

bool NetworkConnection::connectTCP() {
    tcpSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSocket_ == -1) {
        std::cerr << "Error creating TCP socket" << std::endl;
        return false;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);
    inet_pton(AF_INET, serverAddress_.c_str(), &serverAddr.sin_addr);

    if (connect(tcpSocket_, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Error connecting via TCP" << std::endl;
        close(tcpSocket_);
        return false;
    }

    return true;
}

bool NetworkConnection::sendTCP(const std::string& message) {
    ssize_t bytesSent = send(tcpSocket_, message.c_str(), message.size(), 0);
    if (bytesSent == -1) {
        std::cerr << "Error sending TCP message" << std::endl;
        return false;
    }

    return true;
}

std::string NetworkConnection::receiveTCP() {
    char buffer[1024];
    ssize_t bytesReceived = recv(tcpSocket_, buffer, sizeof(buffer), 0);
    if (bytesReceived == -1) {
        std::cerr << "Error receiving TCP message" << std::endl;
        return "";
    }

    return std::string(buffer, bytesReceived);
}

bool NetworkConnection::sendUDP(const std::string& message) {
    udpSocket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket_ == -1) {
        std::cerr << "Error creating UDP socket" << std::endl;
        return false;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);
    inet_pton(AF_INET, serverAddress_.c_str(), &serverAddr.sin_addr);

    ssize_t bytesSent = sendto(udpSocket_, message.c_str(), message.size(), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (bytesSent == -1) {
        std::cerr << "Error sending UDP message" << std::endl;
        return false;
    }

    return true;
}

std::string NetworkConnection::receiveUDP() {
    char buffer[1024];
    socklen_t serverAddrLen = sizeof(struct sockaddr_in);
    ssize_t bytesReceived = recvfrom(udpSocket_, buffer, sizeof(buffer), 0, NULL, NULL);
    if (bytesReceived == -1) {
        std::cerr << "Error receiving UDP message" << std::endl;
        return "";
    }

    return std::string(buffer, bytesReceived);
}

