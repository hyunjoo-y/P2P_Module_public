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

using json = nlohmann::json;

struct ClientInfo {
    int socket;
    std::string clientId;
};

std::unordered_map<std::string, ClientInfo> clients;
std::mutex clientsMutex;

bool receiveCompleteMessage(int socket, std::vector<char>& buffer) {
    int bytesRead;
    char tempBuffer[1024];

    while (true) {
        bytesRead = read(socket, tempBuffer, sizeof(tempBuffer));
        if (bytesRead < 0) {
            std::cerr << "Error reading from socket: " << strerror(errno) << std::endl;
            return false;
        }
        if (bytesRead == 0) {
            std::cerr << "Connection closed by peer." << std::endl;
            return false;
        }

        buffer.insert(buffer.end(), tempBuffer, tempBuffer + bytesRead);

        try {
            json parsedJson = json::parse(buffer);
            return true; // Complete JSON message received
        } catch (const json::parse_error&) {
            // Incomplete JSON, continue reading
            continue;
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
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (auto it = clients.begin(); it != clients.end(); ++it) {
                    if (it->second.socket == clientSocket) {
                        clients.erase(it);
                        break;
                    }
                }
            }
            close(clientSocket);
            break;
        }

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
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clients[clientId] = {clientSocket, clientId};
            }
            std::cout << "Registered client: " << clientId << std::endl;
        } else if (action == "check") {
            std::string targetClientId = message.value("targetClientId", "");
            json response;
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                if (clients.find(targetClientId) != clients.end()) {
                    response = {{"status", "connected"}};
                } else {
                    response = {{"status", "not_connected"}};
                }
            }
            std::string responseStr = response.dump();
            send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
        } else if (action == "send_ice") {
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
    }
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
		return 1;
	}
	int port = std::stoi(argv[1]);
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (serverSocket < 0) {
		std::cerr << "Socket creation error" << std::endl;
		return -1;
	}

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

	std::cout << "Relay node started, waiting for clients..." << std::endl;

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

