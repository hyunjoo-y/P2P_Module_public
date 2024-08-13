#include "KademliaNode.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <NodeID> <ClientID_to_store> <Port> [ClientID_to_search]" << std::endl;
        return 1;
    }

    std::string node_id = argv[1];
    std::string client_id_to_store = argv[2];
    int port = std::stoi(argv[3]);

    std::string client_id_to_search;
    if (argc == 5) {
        client_id_to_search = argv[4];
    }

    // 노드 초기화
    KademliaNode node(node_id, "127.0.0.1", port);

    // 서버 시작
    node.start_server();

    // 네트워크 참여 (bootstrap)
    node.join_network("127.0.0.1", 8082);

    // 부트스트랩 노드가 잠시 기다려서 새로운 노드를 피어로 추가하게 함
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 클라이언트 아이디 저장
    node.store(sha1(client_id_to_store), client_id_to_store);

    // 현재 피어 목록 출력
    node.print_peers();

    // 클라이언트 아이디 검색 (필요한 경우)
    if (!client_id_to_search.empty()) {
        std::string client_id_hash = sha1(client_id_to_search);
        std::string result = node.find_value(client_id_hash);

        if (!result.empty()) {
            std::cout << "Client ID found at: " << result << std::endl;
        } else {
            std::cout << "Client ID not found" << std::endl;
        }
    }

    // 서버가 계속 실행되도록 유지
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }

    return 0;
}

