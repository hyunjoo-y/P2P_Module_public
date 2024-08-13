#ifndef RELAYCOMMUNICATION_HPP
#define RELAYCOMMUNICATION_HPP

#include <iostream>
#include <vector>
#include <thread>
#include <vector>
#include <chrono>
#include <memory>
#include <thread>
#include <string>
#include "json.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <mutex>
#include <condition_variable>
#define BUFFER_SIZE 1024 
#include "../../../ICEGathering/ICECandidatesAdapter.h"

class RelayCommunication {
public:
    RelayCommunication(const std::string& relayIp, int relayPort, const std::string& clientId, std::vector<IceCandidate> sentIceCandidates);
    ~RelayCommunication();
    
    bool connectToRelay();
    void startCommunication(const std::string& targetClientId);

    void sendFileToRelay(const char* filename,  const std::string& toClient);
    std::vector<IceCandidate> getReceivedIceCandidates();
    std::vector<IceCandidate> getIceCandidates();
    void printIceCandidates() const;  // New function to print ICE candidates
    std::pair<std::string, int> getIceRelayInfo();
    void setIceRelayInfo(const std::string& ip, int port);
    
private:
    void handleReceive(std::string fromClient, std::string toClient);
    void sendAll(const void* buffer, size_t length);
    //void sendFileToRelay(const char* filename,  const std::string& toClient);
    ssize_t recvAll(int socket, void* buffer, size_t length);
    void receiveFileFromRelay(std::string filename, std::streamsize fileSize);
    bool receiveCompleteMessage(int sock, std::vector<char>& buffer);
    void handleCommunication(int udpSocket, const std::string& peerIp, int peerPort);
    void registerClient();
    void checkTargetClient(const std::string& targetClientId);
    void sendJsonMessage(const json& message);

    int relaySock;
    std::string relayIp;
    int relayPort;
    std::string clientId;
    mutable std::mutex candidatesMutex;
    std::condition_variable cv;
    std::condition_variable sentCv;
    bool newCandidatesAvailable = false;
    bool newSentCandidatesAvailable = false;

    std::vector<IceCandidate> iceCandidates;  // Assuming iceCandidates is defined elsewhere
    std::vector<IceCandidate> receivedIceCandidates;
    std::vector<IceCandidate> sentIceCandidates;
    
    std::pair<std::string, int> iceRelayInfo;
     mutable std::mutex relayInfoMutex;
    std::condition_variable relay_cv;
    bool newRelayInfoAvailable = false;

    std::string fromClient;
    std::string toClient;
};

#endif // RELAYCOMMUNICATION_HPP
