#ifndef ICE_CANDIDATES_GATHERER_H
#define ICE_CANDIDATES_GATHERER_H

#include "./STUNInterfaces/NATTraversalAdapter.h"
#include "./HostInterfaces/NetworkInterfacesFactory.h"

#include <vector>
#include <string>
#include <cstdint>
#include "json.hpp"

using json = nlohmann::json;

struct IceCandidate {
    std::string type;  // "host", "server reflexive", "peer reflexive", "relay" 등의 유형
    std::string address;  // IPv4 또는 IPv6 주소
    uint16_t port;  // 주소에 연결된 포트 번호
    std::string protocol;  // 프로토콜 (UDP, TCP 등)
    uint32_t priority; // 우선순위

    IceCandidate(const std::string& t, const std::string& addr, uint16_t p, const std::string& proto, int prio)
        : type(t), address(addr), port(p), protocol(proto), priority(prio) {}
       json to_json() const {
        return json{{"type", type}, {"address", address}, {"port", port}, {"protocol", protocol}, {"priority", priority}};
    }
};

class ICECandidatesAdapter {
public:
    ICECandidatesAdapter(const std::string& stunServer, uint16_t stunPort) : stunServer_(stunServer), stunPort_(stunPort) {}
    void gatherCandidates();
    std::vector<IceCandidate> getICECandidates();

private:
    std::vector<IceCandidate> iceCandidates_;
    std::string stunServer_;
    uint16_t stunPort_;
    static uint32_t calculatePriority(uint32_t typePreference, uint32_t localPreference, uint32_t componentId);
    void addICECandidate(const std::string& type, const std::string& address, uint16_t port, const std::string& protocol,int priority);
    void sortCandidatesByPriority();

};

#endif // ICE_CANDIDATES_ADAPTER_H
