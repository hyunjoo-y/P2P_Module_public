// main.cpp
#include "NATTraversalAdapter.h"

int main() {
    NATTraversalAdapter iceAdapter("stun.example.com", 3478);  // STUN 서버 주소와 포트
    iceAdapter.performSTUNOperation();

    // Get the gathered IP addresses
    std::vector<std::pair<std::string, uint16_t> > addresses = iceAdapter.getIPPortAddresses();
    std::cout << "Gathered IP Addresses:" << std::endl;
    for (const auto& address : addresses) {
        std::cout << "IP: " << address.first << ", Port: " << address.second << std::endl;
    }

    return 0;
}

