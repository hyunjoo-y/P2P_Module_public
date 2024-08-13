#include "KademliaNode.h"
#include <iostream>
#include <thread>

int main() {
    // 부트스트랩 노드 초기화
    KademliaNode bootstrapNode("bootstrap", "175.45.203.60", 8082, 8082);

    // 서버 시작
    bootstrapNode.start_server();

    std::cout << "Bootstrap node running at 175.45.203.60:8082" << std::endl;

    // 부트스트랩 노드는 계속 실행 중이어야 함
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }

    return 0;
}

