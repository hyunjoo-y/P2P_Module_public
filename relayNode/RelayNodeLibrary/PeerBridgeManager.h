#ifndef NETWORK_CONNECTION_H
#define NETWORK_CONNECTION_H

#include <string>

class NetworkConnection {
public:
    NetworkConnection(const std::string& serverAddress, int port);
    ~NetworkConnection();

    bool connectTCP();
    bool sendTCP(const std::string& message);
    std::string receiveTCP();

    bool sendUDP(const std::string& message);
    std::string receiveUDP();

private:
    std::string serverAddress_;
    int port_;
    int tcpSocket_;
    int udpSocket_;
};

#endif // NETWORK_CONNECTION_H

