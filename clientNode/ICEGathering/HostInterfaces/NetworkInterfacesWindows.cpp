#ifdef _WIN32
#include "NetworkInterfacesWindows.h"
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <tuple>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

class NetworkInterfacesWindows {
public:
    std::vector<std::tuple<std::string, uint16_t, std::string>> getLocalIPAddresses() const;
};

std::vector<std::tuple<std::string, uint16_t, std::string>> NetworkInterfacesWindows::getLocalIPAddresses() const {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
    
    std::vector<std::tuple<std::string, uint16_t, std::string>> addresses;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    PIP_ADAPTER_ADDRESSES pAddresses = nullptr, pCurrAddresses = nullptr;

    GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, pAddresses, &dwSize);
    pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(dwSize);
    
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, pAddresses, &dwSize) == NO_ERROR) {
        for (pCurrAddresses = pAddresses; pCurrAddresses != nullptr; pCurrAddresses = pCurrAddresses->Next) {
            for (auto *pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast != nullptr; pUnicast = pUnicast->Next) {
                SOCKET sock = socket(pUnicast->Address.lpSockaddr->sa_family, SOCK_DGRAM, IPPROTO_UDP); // For UDP
                char ip[NI_MAXHOST];
                getnameinfo(pUnicast->Address.lpSockaddr, pUnicast->Address.iSockaddrLength, ip, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);

                sockaddr_in servAddr;
                memset(&servAddr, 0, sizeof(servAddr));
                servAddr.sin_family = pUnicast->Address.lpSockaddr->sa_family;
                bind(sock, (sockaddr *)&servAddr, sizeof(servAddr));
                
                sockaddr_in name;
                int nameLen = sizeof(name);
                getsockname(sock, (sockaddr *)&name, &nameLen);
                
                uint16_t port = ntohs(name.sin_port);
                addresses.emplace_back(std::make_tuple(std::string(ip), port, "UDP"));
                closesocket(sock);

                // Repeat for TCP with SOCK_STREAM and IPPROTO_TCP
            }
        }
    }

    if (pAddresses) {
        free(pAddresses);
    }

    WSACleanup();

    return addresses;
}
#endif // _WIN32

