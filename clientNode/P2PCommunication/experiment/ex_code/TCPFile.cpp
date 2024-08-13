#include <iostream>
#include <fstream>
#include <thread>
#include "../../ICEGathering/ICECandidatesAdapter.h"
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define BUFFER_SIZE 1024 * 1024
#define BUF_SIZE 1024
#define END_MARKER "END"

const char* START_OF_FILE = "START_OF_FILE";
const char* END_OF_FILE = "EOF"; // 파일 전송 완료 신호

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void sendAll(int socket, const void* buffer, size_t length) {
    const char* ptr = static_cast<const char*>(buffer);
    while (length > 0) {
        ssize_t sent = send(socket, ptr, length, 0);
        if (sent == -1) {
            std::cerr << "Error sending data!" << std::endl;
            break;
        }
        ptr += sent;
        length -= sent;
    }
}

void sendFileToRelay(const char* filename, int socket, const std::string& toClient){
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
	{"to", toClient},
        {"filename", filename},
        {"fileSize", fileSize}
    };
    std::string metaInfoStr = metaInfo.dump();
    sendAll(socket, metaInfoStr.c_str(), metaInfoStr.length());

    // Allocate buffer dynamically
    char* buffer = new char[BUFFER_SIZE];
    std::streamsize totalSent = 0;
    auto start = std::chrono::high_resolution_clock::now();

    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
        sendAll(socket, buffer, file.gcount());
        totalSent += file.gcount();

        // Display progress
        int progress = static_cast<int>((static_cast<double>(totalSent) / fileSize) * 100);
        std::cout << "\rSending file: " << progress << "% complete" << std::flush;
    }
    std::cout << std::endl;

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "File sent successfully. Time taken: " << duration.count() << " seconds." << std::endl;

    delete[] buffer;
    file.close();
}

void sendFile(const char* filename, int socket) {
    // Send the filename first
    int filenameLength = strlen(filename);
    sendAll(socket, &filenameLength, sizeof(filenameLength));
    sendAll(socket, filename, filenameLength);

    // Open the file
    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file!" << std::endl;
        return;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Send file size
    sendAll(socket, &fileSize, sizeof(fileSize));

    // Allocate buffer dynamically
    char* buffer = new char[BUFFER_SIZE];
    std::streamsize totalSent = 0;
    auto start = std::chrono::high_resolution_clock::now();

    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
        sendAll(socket, buffer, file.gcount());
        totalSent += file.gcount();

        // Display progress
        int progress = static_cast<int>((static_cast<double>(totalSent) / fileSize) * 100);
        std::cout << "\rSending file: " << progress << "% complete" << std::flush;
    }
    std::cout << std::endl;

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "File sent successfully. Time taken: " << duration.count() << " seconds." << std::endl;

    delete[] buffer;
    file.close();
}

void sendText(const char* message, int socket) {
    sendAll(socket, message, strlen(message));
}

ssize_t recvAll(int socket, void* buffer, size_t length) {
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

void receiveFileFromRelay(std::string filename, std::streamsize fileSize,  int socket){
	// Receive meta information
	char metaBuffer[BUFFER_SIZE];
	int metaLength = recv(sock, metaBuffer, BUFFER_SIZE, 0);
	if (metaLength <= 0) {
		std::cerr << "Error receiving file meta information!" << std::endl;
		return;
	}

	// Parse meta information
	std::string metaStr(metaBuffer, metaLength);
	json metaInfo = json::parse(metaStr);
	std::string filename = metaInfo["filename"];
	std::streamsize fileSize = metaInfo["fileSize"];

	std::ofstream file(filename, std::ios::out | std::ios::binary);
	if (!file) {
		std::cerr << "Error creating file!" << std::endl;
		return;
	}

	char* buffer = new char[BUFFER_SIZE];
	std::streamsize totalReceived = 0;

	auto start = std::chrono::high_resolution_clock::now();

	while (totalReceived < fileSize) {
		int bytesReceived = recv(sock, buffer, BUFFER_SIZE, 0);
		if (bytesReceived <= 0) break;
		file.write(buffer, bytesReceived);
		totalReceived += bytesReceived;

		// Display progress
		int progress = static_cast<int>((static_cast<double>(totalReceived) / fileSize) * 100);
		std::cout << "\rReceiving file: " << progress << "% complete" << std::flush;
	}
	std::cout << std::endl;

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> duration = end - start;
	std::cout << "File received successfully. Time taken: " << duration.count() << " seconds." << std::endl;

	delete[] buffer;
	file.close();

}

void receiveFile(int socket) {
    // Receive the filename first
    int filenameLength;
    recvAll(socket, &filenameLength, sizeof(filenameLength));

    char filename[256];
    recvAll(socket, filename, filenameLength);
    filename[filenameLength] = '\0';

    // Receive the file size
    std::streamsize fileSize;
    recvAll(socket, &fileSize, sizeof(fileSize));
      // Start the timer
    auto start = std::chrono::high_resolution_clock::now();

    // Receive the file content
    std::ofstream file(filename, std::ios::out | std::ios::binary);
    if (!file) {
        std::cerr << "Error creating file!" << std::endl;
        return;
    }

    // Allocate buffer dynamically
    char* buffer = new char[BUFFER_SIZE];
    std::streamsize totalReceived = 0;

    while (totalReceived < fileSize) {
        std::streamsize bytesToReceive = std::min(fileSize - totalReceived, static_cast<std::streamsize>(BUFFER_SIZE));
        ssize_t bytesReceived = recvAll(socket, buffer, bytesToReceive);
        if (bytesReceived <= 0) {
            std::cerr << "Error receiving file!" << std::endl;
            break;
        }
        file.write(buffer, bytesReceived);
        totalReceived += bytesReceived;

        // Display progress
        int progress = static_cast<int>((static_cast<double>(totalReceived) / fileSize) * 100);
        std::cout << "\rReceiving file: " << progress << "% complete" << std::flush;
    }
    std::cout << std::endl;

    delete[] buffer;
    file.close();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "File received successfully. Time taken: " << duration.count() << " seconds." << std::endl;
}

void receiveText(int socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytesReceived = recv(socket, buffer, BUFFER_SIZE, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::cout << "Received message: " << buffer << std::endl;
    } else if (bytesReceived < 0) {
        std::cerr << "Error receiving message!" << std::endl;
    }
}


void handleCommunicationThread(int sock) {
	while (true) {
		char input[256];
		std::cout << "Enter 'FILE' to send a file or type a message: ";
		std::cin.getline(input, sizeof(input));
		if (strcmp(input, "FILE") == 0) {
			sendText(input, sock);
			char filename[256];
			std::cout << "Enter the file name to send: ";
			std::cin.getline(filename, sizeof(filename));

			// Send file transfer signal
			std::cout << "Sending file..." << std::endl;
			// Start the timer
			sendFile(filename, sock);
		} else {
			sendText(input, sock);
		}
	}
}

void receiveData(int socket) {
	while (true) {
		char signal[BUFFER_SIZE];
		int signalLength = recv(socket, signal, BUFFER_SIZE, 0);
		if (signalLength > 0) {
			signal[signalLength] = '\0';
			if (strcmp(signal, "FILE") == 0) {
				std::cout << "File transfer signal received. Receiving file..." << std::endl;
				receiveFile(socket);
				std::cout << "File received successfully." << std::endl;
			} else {
				std::cout << "Text message received: " << signal << std::endl;
			}
		} else {
			std::cerr << "Error receiving signal!" << std::endl;
			break;
		}
	}
}

int main() {
    ICECandidatesAdapter iceCandidatesAdapter("175.45.203.60", 3478);
    iceCandidatesAdapter.gatherCandidates();
    const std::vector<IceCandidate>& iceCandidates = iceCandidatesAdapter.getICECandidates();
    int local_port = 0;

    for(const auto& candidate : iceCandidates){
        std::cout << "Type: " << candidate.type << ", Address: " << candidate.address
                  << ", Port: " << candidate.port << ", Protocol: " << candidate.protocol << std::endl;
        if (candidate.type == "srflx" && candidate.protocol == "TCP" && local_port == 0) {
            local_port = candidate.port;
        }
    }

    std::string peer_ip;
    int peer_port;

    std::cout << "Enter peer's IP address: ";
    std::cin >> peer_ip;
    std::cout << "Enter peer's port: ";
    std::cin >> peer_port;

    struct sockaddr_in serv_addr;

    // 서버 주소 설정
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(peer_port);

    if (inet_pton(AF_INET, peer_ip.c_str(), &serv_addr.sin_addr) <= 0)
        error("ERROR invalid server IP address");

    int peer_socket;
    int attempts = 0;
    const int max_attempts = 100;
    bool connected = false;
    while (attempts < max_attempts) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("ERROR opening socket");
            attempts++;
            continue;
        }

        // 소켓 옵션 설정: SO_REUSEADDR
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("ERROR setting SO_REUSEADDR");
            close(sockfd);
            attempts++;
            continue;
        }

        // NAT Traversal을 위해 소켓 바인딩
        struct sockaddr_in local_addr;
        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons(local_port);  // 시스템에서 사용 가능한 포트 할당

        if (bind(sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
            perror("ERROR on binding");
            close(sockfd);
            attempts++;
            continue;
        }
        // 연결 시도
        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("ERROR connecting");
            attempts++;
            std::cout << "Attempt " << attempts << " failed. Retrying..." << std::endl;
	    close(sockfd);
            continue;
        } else {
            connected = true;
            std::cout << "Connected to peer!" << std::endl;

            peer_socket = sockfd;
	    std::thread communicationThread(handleCommunicationThread, sockfd);
            communicationThread.detach(); // 스레드를 백그라운드에서 실행
            break;
        }
    }

    if (!connected) {
        std::cout << "Failed to connect to peer after " << attempts  << " attempts." << std::endl;
    }

    std::thread receiverThread(receiveData, peer_socket);

    receiverThread.join();

    close(peer_socket);

    return 0;
}
