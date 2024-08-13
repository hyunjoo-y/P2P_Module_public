#pragma once
#ifndef NAT_Traversal_Adapter_H
#define NAT_Traversal_Adapter_H

#include <string>
#include <vector>
#include <netinet/in.h>
#include <cstdint>

const uint32_t STUN_MAGIC_COOKIE = 0x2112A442;
const uint16_t STUN_BINDING_REQUEST = 0x0001;
const uint16_t STUN_MAPPED_ADDRESS = 0x0001;
const uint16_t STUN_XOR_MAPPED_ADDRESS = 0x0020;

struct STUNHeader {
    uint16_t type;
    uint16_t length;
    uint32_t magic_cookie;
    uint32_t tid[3];
};

struct STUNAttributeHeader {
    uint16_t type;
    uint16_t length;
};

struct STUNMappedAddressAttribute {
    uint8_t unused;
    uint8_t family;
    uint16_t port;
    struct in_addr ip_addr;
};

class NATTraversalAdapter {
public:
    NATTraversalAdapter(const std::string& server, uint16_t port);
    ~NATTraversalAdapter();
    void performSTUNOperation();
    std::vector<std::pair<std::string, uint16_t>> getUDPIPAddresses() const;
    std::vector<std::pair<std::string, uint16_t>> getTCPIPAddresses() const;

private:
    std::string server_;
    uint16_t port_;
    uint32_t magicCookie_;
    std::vector<std::pair<std::string, uint16_t>> udpIPAddresses_;
    std::vector<std::pair<std::string, uint16_t>> tcpIPAddresses_;

    void createSTUNBindingRequest(std::vector<unsigned char>& buffer);
    void extractSTUNMappedAddress(const std::vector<unsigned char>& buffer, size_t length, bool isUDP);
    void sendUDPRequest();
    void sendTCPRequest();
    void addUDPIPAddress(const std::string& ip, uint16_t port);
    void addTCPIPAddress(const std::string& ip, uint16_t port);
};

#endif // NAT_Traversal_Adapter_H

