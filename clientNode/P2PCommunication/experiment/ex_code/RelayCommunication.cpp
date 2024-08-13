#include "RelayCommunication.hpp"
#include <mutex>

RelayCommunication::RelayCommunication(const std::string& relayIp, int relayPort, const std::string& clientId, std::vector<IceCandidate> sentIceCandidates)
    : relayIp(relayIp), relayPort(relayPort), clientId(clientId), iceCandidates(sentIceCandidates),relaySock(0) {
	     std::cout << "RelayCommunication created with ip: " << relayIp << ", port: " << relayPort
                  << ", clientId: " << clientId << std::endl;
    }

RelayCommunication::~RelayCommunication() {
    if (relaySock != -1) {
        std::cerr << "Closing socket in destructor\n";
        close(relaySock);
    }
}

void RelayCommunication::sendJsonMessage(const json& message) {
    std::string messageStr = message.dump() + "\n";  // 메시지 끝에 '\n' 추가
    ssize_t bytesSent = send(relaySock, messageStr.c_str(), messageStr.length(), 0);
    if (bytesSent < 0) {
        perror("ERROR sending message");
    } else {
        std::cout << "Sent message: " << messageStr << " (bytes sent: " << bytesSent << ")" << std::endl;
    }
}

bool RelayCommunication::connectToRelay() {
	
    struct sockaddr_in relayAddr;
	auto start = std::chrono::high_resolution_clock::now();

    if ((relaySock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error\n";
        return false;
    }
	// 소켓 옵션 설정: SO_REUSEADDR
    int opt = 1;
    if (setsockopt(relaySock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
	    perror("ERROR setting SO_REUSEADDR");
	    return false;
    }

    relayAddr.sin_family = AF_INET;
    relayAddr.sin_port = htons(relayPort);
    if (inet_pton(AF_INET, relayIp.c_str(), &relayAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported\n";
	close(relaySock);
        return false;
    }

    if (connect(relaySock, (struct sockaddr *)&relayAddr, sizeof(relayAddr)) < 0) {
        std::cerr << "Connection Failed\n";
	close(relaySock);
        return false;
    }
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> duration = end - start;
	std::cout << "Relay connection. Time taken: " << duration.count() << " seconds." << std::endl;
    	std::cout << "Connected to relay: " << relayIp << ":" << relayPort << std::endl;


    return true;
}

void RelayCommunication::startCommunication(const std::string& targetClientId) {
	registerClient();
	checkTargetClient(targetClientId);

	std::thread receiverThread(&RelayCommunication::handleReceive, this, clientId, targetClientId);
	receiverThread.detach();
}

void RelayCommunication::sendAll(const void* buffer, size_t length) {
    const char* ptr = static_cast<const char*>(buffer);
    while (length > 0) {
        ssize_t sent = send(relaySock, ptr, length, 0);
        if (sent == -1) {
            std::cerr << "Error sending data!" << std::endl;
            break;
        }
        ptr += sent;
        length -= sent;
    }
}

void RelayCommunication::sendFileToRelay(const char* filename,  const std::string& targetClientId){
    // Send the filename and file size first as a meta information message
    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file!" << std::endl;
        return;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    json metaInfo = {
        {"action", "relay"},
        {"targetClientId", targetClientId},
        {"filename", filename},
        {"fileSize", fileSize}
    };
    std::string metaInfoStr = metaInfo.dump()+ "\n"; 
    sendAll(metaInfoStr.c_str(), metaInfoStr.length());

    // Allocate buffer dynamically
    char* buffer = new char[BUFFER_SIZE];
    std::streamsize totalSent = 0;
    auto start = std::chrono::high_resolution_clock::now();

    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
        sendAll(buffer, file.gcount());
        totalSent += file.gcount();

        // Display progress
        int progress = static_cast<int>((static_cast<double>(totalSent) / fileSize) * 100);
        std::cout << "\rSending file: " << progress << "% complete" << std::flush;
    }
    std::cout << std::endl;

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
	
    std::cout << "File sent successfully. Time taken: " << duration.count() << " seconds." << std::endl;
    // Send END marker
    const char* endMarker = "END\n";
    sendAll(endMarker, strlen(endMarker));


    delete[] buffer;
    file.close();
}

ssize_t RelayCommunication::recvAll(int socket, void* buffer, size_t length) {
        char* ptr = static_cast<char*>(buffer);
        size_t totalReceived = 0;
        while (length > 0) {
                ssize_t received = recv(socket, ptr, length, 0);
                if (received == -1) {
                        std::cerr << "Error receiving data!" << std::endl;
                        break;
                }
                if (received == 0) {
                        break;
                }
                ptr += received;
                length -= received;
                totalReceived += received;
        }
        return totalReceived;
}

void RelayCommunication::receiveFileFromRelay(std::string filename, std::streamsize fileSize){
	std::ofstream file(filename, std::ios::out | std::ios::binary);
	if (!file) {
		std::cerr << "Error creating file!" << std::endl;
		return;
	}


	char* buffer = new char[BUFFER_SIZE];
	std::streamsize totalReceived = 0;
	std::string dataBuffer;

	auto start = std::chrono::high_resolution_clock::now();

	while (totalReceived < fileSize) {
		std::streamsize bytesToReceive = std::min(fileSize - totalReceived, static_cast<std::streamsize>(BUFFER_SIZE));
		ssize_t bytesReceived = recvAll(relaySock, buffer, bytesToReceive);
		if (bytesReceived <= 0) {
			std::cerr << "Error receiving file!" << std::endl;
			break;
		}

		dataBuffer.append(buffer, bytesReceived);

		size_t endPos = dataBuffer.find("END");
		if (endPos != std::string::npos) {
			// "END" 메시지 발견, 실제 파일 데이터 부분만 추출하여 파일에 씁니다.
			file.write(dataBuffer.c_str(), endPos);
			totalReceived += endPos;
			std::cout << "\rReceiving file: 100% complete" << std::flush;
			break;
		} else {
			file.write(dataBuffer.c_str(), dataBuffer.size());
			totalReceived += dataBuffer.size();
		}

		// Display progress
		int progress = static_cast<int>((static_cast<double>(totalReceived) / fileSize) * 100);
		std::cout << "\rReceiving file: " << progress << "% complete" << std::flush;

		dataBuffer.clear();
	}
	std::cout << std::endl;

	delete[] buffer;

	file.close();
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> duration = end - start;
	std::cout << "File received successfully. Time taken: " << duration.count() << " seconds." << std::endl;
}

void RelayCommunication::handleReceive(std::string fromClient, std::string toClient) {
    std::vector<char> buffer;
    std::string peerIp;
    int peerPort = 0;
    bool sendICE = false;

    while (true) {
	    if (relaySock < 0) {
                std::cerr << "Invalid socket in handleReceive\n";
                break;
            }

            buffer.clear();
	    auto start = std::chrono::high_resolution_clock::now();
            if (!receiveCompleteMessage(relaySock, buffer)) {
                std::cerr << "Error receiving complete message." << std::endl;
                break;
            }

            json receivedJson;
            try {
                receivedJson = json::parse(buffer.begin(), buffer.end());
            } catch (const json::parse_error& e) {
                std::cerr << "JSON parse error: " << e.what() << std::endl;
                continue;
            }

        if (receivedJson.contains("action") && receivedJson["action"] == "receive_ice") {
            std::cout << "Received ICE candidates from peer: " << receivedJson.dump() << std::endl;
            std::vector<IceCandidate> recvCandidates;
            for (auto& item : receivedJson["iceCandidates"]) {
                if (item.contains("type") && item["type"].is_string() &&
                    item.contains("address") && item["address"].is_string() &&
                    item.contains("port") && item["port"].is_number_integer() &&
                    item.contains("protocol") && item["protocol"].is_string() &&
                    item.contains("priority") && item["priority"].is_number_integer()) {
                    recvCandidates.emplace_back(item["type"], item["address"], item["port"], item["protocol"], item["priority"]);
                } else {
                    std::cerr << "Invalid ICE candidate received: " << item.dump() << std::endl;
                }
	    }

	    {
		    std::lock_guard<std::mutex> lock(candidatesMutex);
		    receivedIceCandidates.insert(receivedIceCandidates.end(), recvCandidates.begin(), recvCandidates.end());
		    newCandidatesAvailable = true;
	    }
	    cv.notify_all();

	    if (!sendICE) {
		    try {
                    std::cout << "Peer is connected, sending ICE candidates.\n";
                    json iceJson = {
                        {"action", "send_ice"},
                        {"fromClientId", fromClient},
                        {"to", toClient},
                        {"iceCandidates", json::array()}
                    };

		    for (const auto& candidate : iceCandidates) {
			    iceJson["iceCandidates"].push_back(candidate.to_json());
		    }

		    std::string message = iceJson.dump();
		    std::cout << "ice dump: " << message << std::endl;
		    if (send(relaySock, message.c_str(), message.length(), 0) == -1) {
			    throw std::runtime_error("Failed to send data");
		    }
			    
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> duration = end - start;
	std::cout << "ICE Exchanged: Time taken: " << duration.count() << " seconds." << std::endl;
		    sendICE = true;
		    } catch (const std::exception& e) {
			    std::cerr << "Error: " << e.what() << std::endl;
		    }
	    }
	}
	else if (receivedJson.contains("action") && receivedJson["action"] == "relay"){
		std::string filename = "down_";
		if (receivedJson.contains("filename")) {
			filename += receivedJson["filename"].get<std::string>();
		}
		std::streamsize fileSize = 0;
		if (receivedJson.contains("fileSize")) {
			fileSize = receivedJson["fileSize"].get<std::streamsize>();
		}

		receiveFileFromRelay(filename, fileSize);

	}
	else if(receivedJson.contains("status") && receivedJson["status"].is_string()){
		std::string relay_info = receivedJson["status"];
		std::string prefix = "connected_relay ";

		// "connected_relay "가 있는지 확인하고, 있으면 이후 부분을 추출
		if (relay_info.find(prefix) == 0) {
			std::string result = relay_info.substr(prefix.length());

			// ':'의 위치를 찾습니다.
			size_t colon_pos = result.find(':');
			if (colon_pos != std::string::npos) {
				// IP 주소와 포트 번호를 추출합니다.
				std::string ice_relay_ip = result.substr(0, colon_pos);
				int ice_relay_port = std::stoi(result.substr(colon_pos + 1)); 

				{
					std::lock_guard<std::mutex> lock(relayInfoMutex);
					iceRelayInfo = std::make_pair(ice_relay_ip, ice_relay_port);
					newRelayInfoAvailable = true;
				}
				relay_cv.notify_all();

				// 추출된 정보를 출력합니다.
				std::cout << "IP Address: " << ice_relay_ip << std::endl;
				std::cout << "Port: " << ice_relay_port << std::endl;
			} else {
				std::cout << "Invalid format in result" << std::endl;
			}
		}else if(relay_info == "connected"){
			try {
				std::cout << "Peer is connected, sending ICE candidates.\n";
				json iceJson = {
					{"action", "send_ice"},
					{"fromClientId", fromClient},
					{"to", toClient},
					{"iceCandidates", json::array()}
				};

				for (const auto& candidate : iceCandidates) {
					iceJson["iceCandidates"].push_back(candidate.to_json());
				}

				std::string message = iceJson.dump();
				
				if (send(relaySock, message.c_str(), message.length(), 0) == -1) {
					throw std::runtime_error("Failed to send data");
				}
				auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> duration = end - start;
	std::cout << "ICE sending: Time taken: " << duration.count() << " seconds." << std::endl;
				sendICE = true;
				std::cout << "ice dump: " << message << std::endl;
			} catch (const std::exception& e) {
				std::cerr << "Error: " << e.what() << std::endl;
			}
		}else if(relay_info == "not_connected"){
			std::cout << "Peer is not connected.\n";
		} 

		else {
			std::cout << "Invalid status format" << std::endl;
		}
	}	
	else {
		std::cerr << "Received unknown message: " << receivedJson.dump() << std::endl;
	}

	/*	else if (receivedJson.contains("status") && receivedJson["status"].is_string() && receivedJson["status"] == "connected") {
		try {
		std::cout << "Peer is connected, sending ICE candidates.\n";
		json iceJson = {
		{"action", "send_ice"},
		{"fromClientId", fromClient},
		{"to", toClient},
		{"iceCandidates", json::array()}
		};

		for (const auto& candidate : iceCandidates) {
		iceJson["iceCandidates"].push_back(candidate.to_json());
		}

		std::string message = iceJson.dump();
		std::cout << "ice dump: " << message << std::endl;
		if (send(relaySock, message.c_str(), message.length(), 0) == -1) {
		throw std::runtime_error("Failed to send data");
		}
		sendICE = true;
		} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		}
		} else if (receivedJson.contains("status") && receivedJson["status"].is_string() && receivedJson["status"] == "not_connected") {
		std::cout << "Peer is not connected.\n";
		} else {
		std::cerr << "Received unknown message: " << receivedJson.dump() << std::endl;
		}*/
    }
}

std::vector<IceCandidate> RelayCommunication::getReceivedIceCandidates() {
	std::unique_lock<std::mutex> lock(candidatesMutex); // std::unique_lock 사용
	cv.wait(lock, [this] { return newCandidatesAvailable; }); // 조건 변수 대기
	newCandidatesAvailable = false;
	return receivedIceCandidates;
}

std::vector<IceCandidate> RelayCommunication::getIceCandidates() {
	return sentIceCandidates;
}

std::pair<std::string, int> RelayCommunication::getIceRelayInfo() {
	std::unique_lock<std::mutex> lock(relayInfoMutex);
	std::cout << "getIceRelayInfo waiting for newRelayInfoAvailable\n"; // 디버그 메시지 추가
	relay_cv.wait(lock, [this] { return newRelayInfoAvailable; });
	newRelayInfoAvailable = false;
	std::cout << "getIceRelayInfo got newRelayInfo: " << iceRelayInfo.first << ":" << iceRelayInfo.second << std::endl; // 디버그 메시지 추가
	return iceRelayInfo;
}

void RelayCommunication::setIceRelayInfo(const std::string& ip, int port) {
	{
		std::lock_guard<std::mutex> lock(relayInfoMutex);
		iceRelayInfo = std::make_pair(ip, port);
		newRelayInfoAvailable = true;
	}
	std::cout << "setIceRelayInfo set newRelayInfo: " << ip << ":" << port << std::endl; // 디버그 메시지 추가
	relay_cv.notify_all(); // 모든 대기 중인 스레드를 깨움
}

void RelayCommunication::printIceCandidates() const {
	std::lock_guard<std::mutex> lock(candidatesMutex);
	std::cout << "Received ICE Candidates:" << std::endl;
	for (const auto& candidate : receivedIceCandidates) {
		std::cout << candidate.to_json().dump() << std::endl;
	}
}

void RelayCommunication::registerClient() {
	json registerJson = {
		{"action", "register"},
		{"clientId", clientId}
	};
	sendJsonMessage(registerJson);
	// std::string registerMessage = registerJson.dump();
	
}

void RelayCommunication::checkTargetClient(const std::string& targetClientId) {
	json initJson = {
		{"clientId", clientId},
		{"action", "check"},
		{"targetClientId", targetClientId},
		{"relayIp", relayIp},
		{"relayPort", relayPort}
	};
	sendJsonMessage(initJson);

//	std::string initMessage = initJson.dump();
//	 ssize_t bytesSent = send(relaySock, initMessage.c_str(), initMessage.length(), 0);
}

bool RelayCommunication::receiveCompleteMessage(int sock, std::vector<char>& buffer) {
	int bytesRead;
        char tempBuffer[1024];

        while (true) {
            bytesRead = read(sock, tempBuffer, sizeof(tempBuffer));
            if (bytesRead < 0) {
                std::cerr << "Error reading from socket: " << strerror(errno) << std::endl;
                return false;
            } else if (bytesRead == 0) {
                std::cerr << "Socket closed by the peer\n";
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

