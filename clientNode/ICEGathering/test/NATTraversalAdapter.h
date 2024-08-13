#ifndef NAT_TRAVERSAL_ADAPTER_H
#define NAT_TRAVERSAL_ADAPTER_H

#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

// STUN Header
struct STUNHeader {
    uint16_t type;
    uint16_t length;
    uint32_t magic_cookie;
    uint8_t tid[12];
};

// STUN Attribute Header
struct STUNAttributeHeader {
    uint16_t type;
    uint16_t length;
};

// STUN Mapped Address Attribute
struct STUNMappedAddressAttribute {
    uint16_t family;
    uint16_t port;
    struct in_addr ip_addr;
};

// STUN Constants
const uint16_t STUN_BINDING_REQUEST = 0x0001;
const uint16_t STUN_MAPPED_ADDRESS = 0x0001;
const uint16_t STUN_XOR_MAPPED_ADDRESS = 0x0020;
const uint32_t STUN_MAGIC_COOKIE = 0x2112A442;

class NATTraversalAdapter {
public:
    NATTraversalAdapter(const std::string& server, uint16_t port);
    ~NATTraversalAdapter();
    void performSTUNOperation();
    std::vector<std::pair<std::string, uint16_t>> getIPPortAddresses() const;

private:
    std::string server_;
    uint16_t port_;
    uint32_t magicCookie_;
    std::vector<std::pair<std::string, uint16_t>> ipPortAddresses_;

    void createSTUNBindingRequest(std::vector<unsigned char>& buffer);
    void extractMappedAddress(const std::vector<unsigned char>& buffer, size_t length);
    void sendUDPRequest();
    void sendTCPRequest();
};

#endif // NAT_TRAVERSAL_ADAPTER_H

