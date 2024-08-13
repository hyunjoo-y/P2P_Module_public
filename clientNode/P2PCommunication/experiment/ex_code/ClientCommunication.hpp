#ifndef CLIENT_COMMUNICATION_HPP
#define CLIENT_COMMUNICATION_HPP

#include <iostream>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <mutex>
#include <vector>
#include "json.hpp"
#include "../../../ICEGathering/ICECandidatesAdapter.h"

class ClientCommunication {
	public:
		ClientCommunication(std::vector<IceCandidate> receivedIceCandidates, std::vector<IceCandidate> sentIceCandidates);
		~ClientCommunication();

		void handleCommunicationThread();
		void startCommunication();
		bool connectToClient();
		void handleReceive();

	private:
		int clientSock;
		std::string remote_ip;
		int remote_port = 0, local_port = 0;

		void error(const char *msg);

		void sendAll(const void* buffer, size_t length);
		void sendFile(const char* filename);
		void sendText(const char* message);
		ssize_t recvAll(void* buffer, size_t length);
		void receiveFile();
		void receiveText();
};

#endif // FILE_TRANSFER_HPP

