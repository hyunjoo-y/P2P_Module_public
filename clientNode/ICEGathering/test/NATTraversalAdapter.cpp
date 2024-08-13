#include "NATTraversalAdapter.h"

NATTraversalAdapter::NATTraversalAdapter(const std::string& server, uint16_t port) : server_(server), port_(port), magicCookie_(STUN_MAGIC_COOKIE) {}

NATTraversalAdapter::~NATTraversalAdapter() {}

void NATTraversalAdapter::performSTUNOperation() {
    std::cout << "UDP: " << std::endl;
    sendUDPRequest();
    std::cout << "TCP: " << std::endl;
    sendTCPRequest();
}

void NATTraversalAdapter::createSTUNBindingRequest(std::vector<unsigned char>& buffer) {
    buffer.clear();
    buffer.resize(sizeof(STUNHeader));

    auto* header = reinterpret_cast<STUNHeader*>(buffer.data());
    header->type = htons(STUN_BINDING_REQUEST);
    header->length = 0;
    header->magic_cookie = htonl(STUN_MAGIC_COOKIE);

    // Random transaction ID (in production, use more secure random numbers)
    header->tid[0] = rand();
    header->tid[1] = rand();
    header->tid[2] = rand();
}

void NATTraversalAdapter::extractMappedAddress(const std::vector<unsigned char>& buffer, size_t length) {
    size_t pos = sizeof(STUNHeader);
    while (pos + sizeof(STUNAttributeHeader) <= length) {
        const STUNAttributeHeader* attr = reinterpret_cast<const STUNAttributeHeader*>(buffer.data() + pos);
        uint16_t attr_type = ntohs(attr->type);
        uint16_t attr_length = ntohs(attr->length);

        if (attr_type == STUN_MAPPED_ADDRESS || attr_type == STUN_XOR_MAPPED_ADDRESS) {
            if (attr_length >= sizeof(STUNMappedAddressAttribute) - sizeof(STUNAttributeHeader)) {
                const STUNMappedAddressAttribute* mapped = reinterpret_cast<const STUNMappedAddressAttribute*>(attr + 1);

                uint16_t actual_port = ntohs(mapped->port) ^ (STUN_MAGIC_COOKIE >> 16);
                struct in_addr actual_ip;
                actual_ip.s_addr = mapped->ip_addr.s_addr ^ htonl(STUN_MAGIC_COOKIE);

                // Store the address
                ipPortAddresses_.emplace_back(std::make_pair(inet_ntoa(actual_ip), actual_port));
                return;
            }
        }
        pos += sizeof(STUNAttributeHeader) + attr_length;
    }
    std::cout << "MAPPED-ADDRESS attribute not found.\n";
}

void NATTraversalAdapter::sendUDPRequest() {
    // Creating UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("Failed to create UDP socket");
    }

    // Setting up the STUN server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    server_addr.sin_addr.s_addr = inet_addr(server_.c_str());  // In production, use DNS resolution

    // Creating a STUN binding request
    std::vector<unsigned char> request;
    createSTUNBindingRequest(request);

    // Sending STUN request to the server
    if (sendto(sockfd, request.data(), request.size(), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        throw std::runtime_error("Failed to send data via UDP");
    }

    // Receiving STUN response from the server
    std::vector<unsigned char> buffer(1024);
    socklen_t len = sizeof(server_addr);
    ssize_t n = recvfrom(sockfd, buffer.data(), buffer.size(), 0, (struct sockaddr*)&server_addr, &len);
    if (n > 0) {
        extractMappedAddress(buffer, n);
    } else {
        close(sockfd);
        throw std::runtime_error("Failed to receive data via UDP");
    }

    // Closing socket
    close(sockfd);
}

void NATTraversalAdapter::sendTCPRequest() {
    // Creating TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("Failed to create TCP socket");
    }

    // Setting up the STUN server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    server_addr.sin_addr.s_addr = inet_addr(server_.c_str());  // In production, use DNS resolution


    // Connecting to the STUN server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        throw std::runtime_error("Failed to connect to STUN server via TCP");
    }

    // Creating a STUN binding request
    std::vector<unsigned char> request;
    createSTUNBindingRequest(request);

    // Sending STUN request to the server
    if (write(sockfd, request.data(), request.size()) < 0) {
        close(sockfd);
        throw std::runtime_error("Failed to send data via TCP");
    }

    // Receiving STUN response from the server
    std::vector<unsigned char> buffer(1024);
    ssize_t n = read(sockfd, buffer.data(), buffer.size());
    if (n > 0) {
        extractMappedAddress(buffer, n);
    } else {
        close(sockfd);
        throw std::runtime_error("Failed to receive data via TCP");
    }

    // Closing socket
    close(sockfd);
}

std::vector<std::pair<std::string, uint16_t>> NATTraversalAdapter::getIPPortAddresses() const {
    return ipPortAddresses_;
}
