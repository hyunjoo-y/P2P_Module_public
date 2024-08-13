// NetworkInterfacesUnix.cpp

#include "NetworkInterfacesUnix.h"
#include <vector>
#include <utility>
#include <string>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/socket.h>
#include <tuple>

std::vector<std::tuple<std::string, uint16_t, std::string>> NetworkInterfacesUnix::getLocalIPAddresses() const {
   std::vector<std::tuple<std::string, uint16_t, std::string>> addresses;

    struct ifaddrs *ifap, *ifa;
    if (getifaddrs(&ifap) == -1) {
        std::cerr << "Failed to get network interfaces\n";
        return addresses;
    }

    for (ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;

        int family = ifa->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            char host[NI_MAXHOST];

            int ret = getnameinfo(ifa->ifa_addr,
                                  (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                                  host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
            if (ret != 0) {
                std::cerr << "Failed to get IP address: " << gai_strerror(ret) << "\n";
                continue;
            }

            // UDP 포트 동적 할당 및 조회
            int udpSock = socket(family, SOCK_DGRAM, 0);
            if (udpSock < 0) continue;

            if (bind(udpSock, ifa->ifa_addr,
                     (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)) == 0) {
                struct sockaddr_storage ss;
                socklen_t len = sizeof(ss);
                if (getsockname(udpSock, (struct sockaddr *)&ss, &len) == 0) {
                    uint16_t port = ntohs(((struct sockaddr_in *)&ss)->sin_port);
                    addresses.emplace_back(std::string(host), port, "UDP");
                }
            }
            close(udpSock);

            // TCP 포트 동적 할당 및 조회
            int tcpSock = socket(family, SOCK_STREAM, 0);
            if (tcpSock < 0) continue;

            if (bind(tcpSock, ifa->ifa_addr,
                     (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)) == 0) {
                if (listen(tcpSock, 1) == 0) {
                    struct sockaddr_storage ss;
                    socklen_t len = sizeof(ss);
                    if (getsockname(tcpSock, (struct sockaddr *)&ss, &len) == 0) {
                        uint16_t port = ntohs(((struct sockaddr_in *)&ss)->sin_port);
                        addresses.emplace_back(std::string(host), port, "TCP");
                    }
                }
            }
            close(tcpSock);
        }
    }

    freeifaddrs(ifap);
    return addresses;
}

