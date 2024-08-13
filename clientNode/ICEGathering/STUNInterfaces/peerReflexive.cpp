#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

// STUN 서버의 IP 주소와 포트 번호
const std::string STUN_SERVER_IP = "175.45.203.60";
const int STUN_SERVER_PORT = 3478;

// STUN 메시지 타입
enum class StunMessageType {
    BindingRequest = 0x0001,
    BindingResponse = 0x0101,
    BindingErrorResponse = 0x0111
};

// STUN 메시지 구조체
struct StunMessage {
    uint16_t messageType;
    uint16_t messageLength;
    uint32_t magicCookie;
    uint32_t transactionID[3];
};

// STUN 서버와 통신하여 Peer Reflexive Candidates를 수집하는 함수
std::vector<std::pair<std::string, uint16_t>> getPeerReflexiveCandidates() {
    std::vector<std::pair<std::string, uint16_t>> candidates;

    // STUN 서버와 소켓 통신을 위한 소켓 생성
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return candidates;
    }

    // STUN 서버의 주소 설정
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(STUN_SERVER_PORT);
    inet_pton(AF_INET, STUN_SERVER_IP.c_str(), &serverAddr.sin_addr);

    // STUN Binding Request 메시지 생성
    StunMessage request;
    request.messageType = static_cast<uint16_t>(StunMessageType::BindingRequest);
    request.messageLength = 0;
    request.magicCookie = 0x2112A442;
    request.transactionID[0] = rand();
    request.transactionID[1] = rand();
    request.transactionID[2] = rand();

    // STUN 서버로 Binding Request 메시지 전송
    ssize_t bytesSent = sendto(sockfd, &request, sizeof(request), 0, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (bytesSent < 0) {
        std::cerr << "Error sending request to STUN server" << std::endl;
        close(sockfd);
        return candidates;
    }

    // STUN 서버로부터 응답 수신
    StunMessage response;
    socklen_t serverAddrLen = sizeof(serverAddr);
    ssize_t bytesReceived = recvfrom(sockfd, &response, sizeof(response), 0, reinterpret_cast<struct sockaddr*>(&serverAddr), &serverAddrLen);
    if (bytesReceived < 0) {
        std::cerr << "Error receiving response from STUN server" << std::endl;
        close(sockfd);
        return candidates;
    }

    // Peer Reflexive Candidates 추출
    if (response.messageType == static_cast<uint16_t>(StunMessageType::BindingResponse)) {
        // STUN Binding Response 메시지에서 주소와 포트 번호 추출
        std::string peerAddress = inet_ntoa(serverAddr.sin_addr);
        uint16_t peerPort = ntohs(serverAddr.sin_port);
        candidates.emplace_back(peerAddress, peerPort);
    }

    // 소켓 닫기
    close(sockfd);

    return candidates;
}

int main() {
    // Peer Reflexive Candidates 수집
    std::vector<std::pair<std::string, uint16_t>> peerCandidates = getPeerReflexiveCandidates();

    // 수집된 Peer Reflexive Candidates 출력
    std::cout << "Peer Reflexive Candidates:" << std::endl;
    for (const auto& candidate : peerCandidates) {
        std::cout << candidate.first << ":" << candidate.second << std::endl;
    }

    return 0;
}

