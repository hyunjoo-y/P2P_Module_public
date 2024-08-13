#include "KademliaNode.h"

#include <iostream>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "json.hpp"
#include <thread>
#include <mutex>
#include <iomanip>
#include <sstream>

#define BUFFER_SIZE 1024

using json = nlohmann::json;

struct ClientInfo {
        int socket;
        std::string clientId;
};

std::unordered_map<std::string, ClientInfo> clients;
std::mutex clientsMutex;
//KademliaNode node("", "", 0); // 임시 초기화
KademliaNode* node = nullptr;  // 포인터로 변경
std::string relay_ice_info = "";

std::string execCommand(const char* cmd) {
        std::array<char, 128> buffer;
        std::string result;
        std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
        if (!pipe) {
                throw std::runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
        }
        return result;
}

std::string getExternalIPAddress() {
        std::string result = execCommand("curl -s ifconfig.me");
        if (!result.empty() && result.back() == '\n') {
                result.pop_back(); // Remove the newline character
        }
        return result;
}
// 소켓 번호를 통한 클라이언트 ID 검색 함수
std::string getClientIdBySocket(int sock) {
    for (const auto& clientPair : clients) {
        if (clientPair.second.socket == sock) {
            return clientPair.first; // clientId 반환
        }
    }
    return ""; // 클라이언트를 찾지 못하면 빈 문자열 반환
}

// 클라이언트 제거 함수
void removeClientBySocket(int sock) {
    std::string clientId = getClientIdBySocket(sock);
    if (!clientId.empty()) {
        close(clients[clientId].socket); // 소켓 닫기
        clients.erase(clientId);         // 맵에서 클라이언트 삭제
        std::cout << "Client " << clientId << " removed from client list." << std::endl;
    }
}

void sendAll(int clientSocket, const void* buffer, size_t length) {
         const char* ptr = static_cast<const char*>(buffer);
    while (length > 0) {
        ssize_t sent = send(clientSocket, ptr, length, 0);
        if (sent == -1) {
            std::cerr << "Error sending data!" << std::endl;
            removeClientBySocket(clientSocket);
            break;
        }
        ptr += sent;
        length -= sent;
    }
}



ssize_t recvAll(int socket, void* buffer, size_t length) {
        char* ptr = static_cast<char*>(buffer);
        size_t totalReceived = 0;
        while (length > 0) {
                ssize_t received = recv(socket, ptr, length, 0);
                if (received == -1) {
                        removeClientBySocket(socket);
                        std::cerr << "Error receiving data!" << std::endl;
                        break;
                }
                if (received == 0) {
                        removeClientBySocket(socket);
                        break;
                }
                ptr += received;
                length -= received;
                totalReceived += received;
        }
        return totalReceived;
}


bool receiveCompleteMessage(int sock, std::vector<char>& buffer) {
        int bytesRead;
        char tempBuffer[1024];

        while (true) {
            bytesRead = read(sock, tempBuffer, sizeof(tempBuffer));
            if (bytesRead < 0) {
                std::cerr << "Error reading from socket: " << strerror(errno) << std::endl;
                removeClientBySocket(sock);
                return false;
            } else if (bytesRead == 0) {
                std::cerr << "Socket closed by the peer\n";
                removeClientBySocket(sock);
                return false;
            }

            buffer.insert(buffer.end(), tempBuffer, tempBuffer + bytesRead);

            try {
                json parsedJson = json::parse(buffer);
                return true; // Complete JSON message received
            } catch (const json::parse_error& e) {
                if (e.id != 101) { // Ignore unexpected end of input errors
                    std::cerr << "JSON parse error: " << e.what() << std::endl;
                    return false;
                }
            }
        }

        return true;

}
void handleClient(int clientSocket) {
        std::vector<char> buffer;
        while (true) {
                buffer.clear();

                if (!receiveCompleteMessage(clientSocket, buffer)) {
                        std::cerr << "Error receiving complete message." << std::endl;
                        break;
                }

                std::string rawMessage(buffer.begin(), buffer.end());
                std::cout << "Received raw message: " << rawMessage << std::endl;

                json message;
                try {
                        message = json::parse(buffer.begin(), buffer.end());
                } catch (const json::parse_error& e) {
                        std::cerr << "JSON parse error: " << e.what() << std::endl;
                        continue;
                }

                std::string action = message.value("action", "");

                std::cout << "action" << action << std::endl;

                if (action.empty()) {
                        std::cerr << "Received message without action: " << message.dump() << std::endl;
                        continue;
                }

                if (action == "register") {
                        std::string clientId = message.value("clientId", "");
                        if (clientId.empty()) {
                                std::cerr << "Received register message without clientId: " << message.dump() << std::endl;
                                continue;
                        }
                        else{

                                std::lock_guard<std::mutex> lock(clientsMutex);

                                // 클라이언트 ID가 이미 존재하는 경우 소켓 업데이트
                                auto it = clients.find(clientId);
                                if (it != clients.end()) {
                                        close(it->second.socket); // 기존 소켓 닫기
                                        it->second.socket = clientSocket; // 새로운 소켓으로 업데이트
                                        std::cout << "Updated client socket for: " << clientSocket << std::endl;
                                } else {
                                        // 클라이언트 새로 등록
                                        clients[clientId] = {clientSocket, clientId};
                                        std::cout << "Registered new client: " << clientId << " " << clientSocket  << std::endl;
                                }
                                // 클라이언트 아이디 저장
                                node->store(sha1(clientId), clientId);

                                // 현재 피어 목록 출력
                                node->print_peers();
                        }

                } else if (action == "check") {
                        std::string targetClientId = message.value("targetClientId", "");
                        json response;
                        {
                                std::lock_guard<std::mutex> lock(clientsMutex);
                                if (clients.find(targetClientId) != clients.end()) {
                                        response = {{"status", "connected"}};
                                } else {
                                        // 클라이언트 아이디 검색 (필요한 경우)
                                        if (!targetClientId.empty()) {
                                                std::string client_id_hash = sha1(targetClientId);
                                        // relay 정보 받아오기
                                                std::string result = node->find_value(client_id_hash);
                                                if (!result.empty()) {
                                                        std::string relay_info = "connected_relay " + result;
                                                        std::cout << "Client ID found at: " << result << std::endl;
                                                        response = {{"status", relay_info}};
                                                } else {
                                                        std::cout << "Client ID not found" << std::endl;
                                                        response = {{"status", "not_connected"}};
                                                }

                                        }else{
                                                response = {{"status", "not_connected"}};
                                        }

                                }
                        }
                        std::string responseStr = response.dump();
                        send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
                }
                else if (action == "send_ice") {
                        std::string toClientId = message.value("to", "");
                        json iceCandidates = message.value("iceCandidates", json::array());
                        if (toClientId.empty() || iceCandidates.is_null()) {
                                std::cerr << "Invalid send_ice message: " << message.dump() << std::endl;
                                continue;
                        }
                        std::lock_guard<std::mutex> lock(clientsMutex);
                        if (clients.find(toClientId) != clients.end()) {
                                int toClientSocket = clients[toClientId].socket;
                                json forwardMessage = {
                                        {"action", "receive_ice"},
                                        {"fromClientId", message.value("fromClientId", "")},
                                        {"iceCandidates", iceCandidates}
                                };
                                std::string forwardMessageStr = forwardMessage.dump();
                                ssize_t sentBytes = send(toClientSocket, forwardMessageStr.c_str(), forwardMessageStr.length(), 0);
                                if (sentBytes < 0) {
                                        std::cerr << "Error sending data to client " << toClientId << ": " << strerror(errno) << std::endl;
                                } else if (sentBytes != static_cast<ssize_t>(forwardMessageStr.length())) {
                                        std::cerr << "Warning: Incomplete data sent to client " << toClientId << std::endl;
                                }
                        }
                }
                else if (action == "relay") {
                        char re_buffer[1024];
                        std::string targetClientId = message.value("targetClientId", "");
                        int toClientSocket = clients[targetClientId].socket;

                        std::streamsize fileSize = message.value("fileSize",0);
                        sendAll(toClientSocket, rawMessage.c_str(), rawMessage.size());

                        std::cout << "Relaying file: " << toClientSocket << " "  << fileSize << " bytes) to client: " << targetClientId << std::endl;

                        std::streamsize totalReceived = 0;
                        while (totalReceived < fileSize) {
                                std::streamsize bytesToReceive = std::min(fileSize - totalReceived, static_cast<std::streamsize>(BUFFER_SIZE));

                                ssize_t bytesRead = recvAll(clientSocket, re_buffer, sizeof(bytesToReceive));
                                if (bytesRead <= 0) {
                                        std::cerr << "Error receiving file data." << std::endl;
                                        return;
                                }

                                sendAll(toClientSocket, re_buffer, bytesRead );
                                totalReceived += bytesRead;

                                    // 버퍼를 다시 0으로 초기화하여 "비우기"
                                memset(re_buffer, 0, BUFFER_SIZE);

                                // 진행 상황 출력
                                int progress = static_cast<int>((static_cast<double>(totalReceived) / fileSize) * 100);
                                std::cout << "\rRelaying file: " << progress << "% complete" << std::flush;
                        }
                        std::cout << std::endl;
                        std::cout << "File relayed successfully to client: " << targetClientId << std::endl;

                }

        }
}
int main(int argc, char* argv[]) {
        if (argc < 3) {
                std::cerr << "Usage: " << argv[0] << " <NodeID> <port>" << std::endl;
                return 1;
        }

        std::string node_id = argv[1];
        int port = std::stoi(argv[2]);

        int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

        if (serverSocket < 0) {
                std::cerr << "Socket creation error" << std::endl;
                return -1;
        }


        std::string externalIPAddress = getExternalIPAddress();


        // 노드 초기화
        node = new KademliaNode(node_id, externalIPAddress.c_str(), port, port+5);

        // 서버 시작
        std::cout << "Starting server..." << std::endl;
        node->start_server();

        // 네트워크 참여 (bootstrap)
        std::cout << "Joining network via bootstrap node..." << std::endl;
       // node->join_network(externalIPAddress.c_str(), 8082);
        node->join_network("175.45.203.60", 8082);

        // 부트스트랩 노드가 잠시 기다려서 새로운 노드를 피어로 추가하게 함
        std::this_thread::sleep_for(std::chrono::seconds(1));

        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
                std::cerr << "Bind failed" << std::endl;
                close(serverSocket);
                return -1;
        }

        if (listen(serverSocket, 10) < 0) {
                std::cerr << "Listen failed" << std::endl;
                close(serverSocket);
                return -1;
        }

        std::cout << "Relay node started, waiting for clients…" << std::endl;

        while (true) {
                struct sockaddr_in clientAddr;
                socklen_t clientAddrLen = sizeof(clientAddr);
                int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
                if (clientSocket < 0) {
                        std::cerr << "Accept failed" << std::endl;
                        continue;
                }
                std::thread clientThread(handleClient, clientSocket);
                clientThread.detach();
        }

        close(serverSocket);
        return 0;
}
