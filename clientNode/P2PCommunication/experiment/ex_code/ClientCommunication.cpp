#include "ClientCommunication.hpp"
#define BUFFER_SIZE 1024
#include <mutex>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>    // fcntl, F_GETFL, F_SETFL, O_NONBLOCK 정의
#include <unistd.h>   // close 함수 정의
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <cerrno>

ClientCommunication::ClientCommunication(std::vector<IceCandidate> receivedIceCandidates, std::vector<IceCandidate> sentIceCandidates)
	:clientSock(0) {
		for(const auto& candidates : sentIceCandidates){
			if (candidates.type == "srflx" && candidates.protocol == "TCP"  && local_port == 0){
				local_port = candidates.port;
				break;
			}
		}

		for(const auto& candidates :receivedIceCandidates){
			if (candidates.type == "srflx" && candidates.protocol == "TCP"  && remote_port == 0){
				remote_ip = candidates.address;
				remote_port = candidates.port;
				break;
			}
		}


	}

ClientCommunication::~ClientCommunication() {
	close(clientSock);
}

void ClientCommunication::error(const char *msg) {
    perror(msg);
    exit(1);
}

void ClientCommunication::startCommunication(){
	std::thread receiverThread(&ClientCommunication::handleReceive, this);
	receiverThread.detach();

//	std::thread communicationThread(&ClientCommunication::handleCommunicationThread, this);
//	communicationThread.detach();
//	handleCommunicationThread();
	// receiverThread.join();
}

/*bool ClientCommunication::connectToClient() {
	struct sockaddr_in client_addr;
	// Client 주소 설정

	auto start = std::chrono::high_resolution_clock::now();
	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(remote_port);

	if (inet_pton(AF_INET, remote_ip.c_str(), &client_addr.sin_addr) <= 0)
		error("ERROR invalid server IP address");

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
		if (connect(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
			perror("ERROR connecting");
			attempts++;
			std::cout << "Attempt " << attempts << " failed. Retrying..." << std::endl;
			close(sockfd);
			continue;
		} else {
			
   			auto end = std::chrono::high_resolution_clock::now();
    			std::chrono::duration<double> duration = end - start;
    			std::cout << "Client Connection successfully. Time taken: " << duration.count() << " seconds." << std::endl;
			
			connected = true;

			clientSock = sockfd;
			std::thread communicationThread(&ClientCommunication::handleCommunicationThread, this);
			communicationThread.detach(); // 스레드를 백그라운드에서 실행
			return true;
		}
		// 시도 후 시간 확인
	        auto current_time = std::chrono::high_resolution_clock::now();
	        std::chrono::duration<double> elapsed = current_time - start;
	        if (elapsed.count() > 2.0) {
	            std::cout << "100 attempts not completed within 2 seconds." << std::endl;
	            return false;
	        }
	}

	return false;

}*/

bool ClientCommunication::connectToClient() {
	struct sockaddr_in client_addr;
	// 클라이언트 주소 설정

	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(remote_port);

	if (inet_pton(AF_INET, remote_ip.c_str(), &client_addr.sin_addr) <= 0) {
		error("ERROR invalid server IP address");
		return false;
	}

	int attempts = 0;
	const int max_attempts = 100;

	auto overallStart = std::chrono::high_resolution_clock::now(); // 전체 시도 타이머 시작

	while (attempts < max_attempts) {
		auto current_time = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> overallElapsed = current_time - overallStart;
		if (overallElapsed.count() > 2.0) {
			std::cout << "100 attempts not completed within 2 seconds." << std::endl;
			return false;
		}

		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) {
			perror("ERROR opening socket");
			attempts++;
			continue;
		}

		// 소켓을 논블로킹 모드로 설정
		int flags = fcntl(sockfd, F_GETFL, 0);
		fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

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

		// 연결 시도 (논블로킹 모드)
		if (connect(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
			if (errno != EINPROGRESS) {
				perror("ERROR connecting");
				attempts++;
				close(sockfd);
				continue;
			}

			// 논블로킹 연결 진행 중
			fd_set writefds;
			FD_ZERO(&writefds);
			FD_SET(sockfd, &writefds);

			struct timeval timeout;
			timeout.tv_sec = 2;
			timeout.tv_usec = 0;

			int selectResult = select(sockfd + 1, NULL, &writefds, NULL, &timeout);
			if (selectResult <= 0) {
				std::cerr << "Connection attempt timed out." << std::endl;
				close(sockfd);
				attempts++;
				continue;
			}

			int sockError;
			socklen_t len = sizeof(sockError);
			if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &sockError, &len) < 0 || sockError != 0) {
				std::cerr << "Error in connection: " << strerror(sockError) << std::endl;
				close(sockfd);
				attempts++;
				continue;
			}
		}else{

			// 연결 성공
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> duration = end - overallStart;
			std::cout << "Client Connection successfully. Time taken: " << duration.count() << " seconds." << std::endl;

			clientSock = sockfd;
			std::thread communicationThread(&ClientCommunication::handleCommunicationThread, this);
			communicationThread.detach(); // 스레드를 백그라운드에서 실행
			return true;
		}
	}
	std::cout << "Maximum attempts reached without successful connection." << std::endl;
	return false;

}

void ClientCommunication::sendAll(const void* buffer, size_t length) {
	const char* ptr = static_cast<const char*>(buffer);
	while (length > 0) {
		ssize_t sent = send(clientSock, ptr, length, 0);
		if (sent == -1) {
			std::cerr << "Error sending data!" << std::endl;
			break;
		}
        ptr += sent;
        length -= sent;
    }
}

void ClientCommunication::sendText(const char* message) {
    sendAll(message, strlen(message));
}

void ClientCommunication::sendFile(const char* filename) {
    // Send the filename first
    int filenameLength = strlen(filename);
    sendAll(&filenameLength, sizeof(filenameLength));
    sendAll(filename, filenameLength);

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
    sendAll(&fileSize, sizeof(fileSize));

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

    delete[] buffer;
    file.close();
}

ssize_t ClientCommunication::recvAll(void* buffer, size_t length) {
        char* ptr = static_cast<char*>(buffer);
        size_t totalReceived = 0;
        while (length > 0) {
                ssize_t received = recv(clientSock, ptr, length, 0);
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

void ClientCommunication::receiveFile() {
    // Receive the filename first
    int filenameLength;
    recvAll(&filenameLength, sizeof(filenameLength));

    char filename[256];
    recvAll(filename, filenameLength);
    filename[filenameLength] = '\0';

    std::string newFilename = "down_" + std::string(filename);

    // Receive the file size
    std::streamsize fileSize;
    recvAll(&fileSize, sizeof(fileSize));
      // Start the timer
    auto start = std::chrono::high_resolution_clock::now();

    // Receive the file content
    std::ofstream file(newFilename, std::ios::out | std::ios::binary);
    if (!file) {
        std::cerr << "Error creating file!" << std::endl;
        return;
    }

    // Allocate buffer dynamically
    char* buffer = new char[BUFFER_SIZE];
    std::streamsize totalReceived = 0;

    while (totalReceived < fileSize) {
        std::streamsize bytesToReceive = std::min(fileSize - totalReceived, static_cast<std::streamsize>(BUFFER_SIZE));
        ssize_t bytesReceived = recvAll(buffer, bytesToReceive);
        if (bytesReceived <= 0) {
            std::cerr << "Error receiving file!" << std::endl;
            break;
        }
        file.write(buffer, bytesReceived);
        totalReceived += bytesReceived;

        // Display progress
        //int progress = static_cast<int>((static_cast<double>(totalReceived) / fileSize) * 100);
        //std::cout << "\rReceiving file: " << progress << "% complete" << std::flush;
    }
   // std::cout << std::endl;

    delete[] buffer;
    file.close();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "File received successfully. Time taken: " << duration.count() << " seconds." << std::endl;
}

void ClientCommunication::receiveText() {
    char buffer[BUFFER_SIZE];
    ssize_t bytesReceived = recv(clientSock, buffer, BUFFER_SIZE, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::cout << "Received message: " << buffer << std::endl;
    } else if (bytesReceived < 0) {
        std::cerr << "Error receiving message!" << std::endl;
    }
}

/*void ClientCommunication::handleCommunicationThread() {
        while (true) {
                char input[256];
                std::cout << "Enter 'FILE' to send a file or type a message: ";
		std::cin.getline(input, sizeof(input));

		if (std::cin.eof()) {
			std::cerr << "EOF reached. Exiting communication thread." << std::endl;
			break;
		}

		if (std::cin.fail()) {
			std::cin.clear(); // Clear the error flag
			std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore the rest of the line
			std::cerr << "Input error. Please try again." << std::endl;
			continue;
		}
		if (strcmp(input, "FILE") == 0) {
                        sendText(input);
                        char filename[256];
                        std::cout << "Enter the file name to send: ";
                        std::cin.getline(filename, sizeof(filename));

			if (std::cin.fail()) {
				std::cin.clear(); // Clear the error flag
				std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore the rest of the line
				std::cerr << "Input error. Please try again." << std::endl;
				continue;
			}

                        // Send file transfer signal
                        std::cout << "Sending file..." << std::endl;
                        // Start the timer
                        sendFile(filename);
                } else {
                        sendText(input);
                }
        }
}*/

void ClientCommunication::handleCommunicationThread() {
    while (true) {
        char input[256];
        std::cout << "Enter 'FILE' to send a file or type a message: ";
        std::cin.getline(input, sizeof(input));

        if (std::cin.eof()) {
            std::cerr << "EOF reached. Exiting communication thread." << std::endl;
            break;
        }

        if (std::cin.fail()) {
            std::cin.clear(); // Clear the error flag
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore the rest of the line
            std::cerr << "Input error. Please try again." << std::endl;
            continue;
        }

        if (strcmp(input, "FILE") == 0) {
            sendText(input);
            char filename[256];
            std::cout << "Enter the file name to send: ";
            std::cin.getline(filename, sizeof(filename));

            if (std::cin.eof()) {
                std::cerr << "EOF reached. Exiting communication thread." << std::endl;
                break;
            }

            if (std::cin.fail()) {
                std::cin.clear(); // Clear the error flag
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore the rest of the line
                std::cerr << "Input error. Please try again." << std::endl;
                continue;
            }

            // Send file transfer signal
            std::cout << "Sending file..." << std::endl;
            // Start the timer
            sendFile(filename);
        } else {
            sendText(input);
        }
    }
}


/*void ClientCommunication::handleCommunicationThread() {
	while (true) {
		std::string input;
		std::cout << "Enter 'FILE' to send a file or type a message: ";
		std::getline(std::cin, input);

		if (input == "FILE") {
			std::string filename;
			std::cout << "Enter the file name to send: ";
			std::getline(std::cin, filename);

			// Send file transfer signal
			std::cout << "Sending file..." << std::endl;
			sendFile(filename.c_str());
		} else {
			sendText(input.c_str());
		}
	}
}*/


void ClientCommunication::handleReceive() {
        while (true) {
                char signal[BUFFER_SIZE];
                int signalLength = recv(clientSock, signal, BUFFER_SIZE, 0);
                if (signalLength > 0) {
                        signal[signalLength] = '\0';
                        if (strcmp(signal, "FILE") == 0) {
                                std::cout << "File transfer signal received. Receiving file..." << std::endl;
                                receiveFile();
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
