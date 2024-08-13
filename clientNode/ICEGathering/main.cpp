#include "ICECandidatesAdapter.h"
#include <iostream>

int main() {
    // STUN 서버의 주소와 포트를 지정하여 ICECandidatesAdapter 객체 생성
    ICECandidatesAdapter adapter("175.45.203.60", 3478);
    adapter.gatherCandidates();

    // 수집된 ICE candidates 출력
    const auto& candidates = adapter.getICECandidates();
    for (const auto& candidate : candidates) {
        std::cout << "Type: " << candidate.type << ", Address: " << candidate.address 
                  << ", Port: " << candidate.port << ", Protocol: " << candidate.protocol << std::endl;
    }

    return 0;
}

