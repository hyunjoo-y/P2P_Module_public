#include "RPCClient.hpp"
#include "RelayCommunication.hpp"
#include "ClientCommunication.hpp"
#include <iostream>
#include <chrono>
#include <atomic>

std::atomic<bool> relayInfoReceived(false);

std::vector<IceCandidate> receivedIceCandidates;
std::vector<IceCandidate> iceCandidates;

void connectToRelay(const std::string& relay, RPCClient& rpcClient, const std::string& clientId, const std::string& targetClientId, const std::vector<IceCandidate>& iceCandidates) {
	try {
		std::pair<std::string, int> ipPort = rpcClient.getIpPort(relay);
		std::string relayIp = ipPort.first;
		int relayPort = ipPort.second;
		auto relayComm = std::make_shared<RelayCommunication>(relayIp, relayPort, clientId, iceCandidates);
		if (relayComm->connectToRelay()) {
			std::cout << "Successfully connected to relay: " << relay << std::endl;
			relayComm->startCommunication(targetClientId);

			// 하나의 릴레이에서만 정보를 받도록 함
			if (!relayInfoReceived.exchange(true)) {
				receivedIceCandidates = relayComm->getReceivedIceCandidates();
				std::cout << "ICE Candidates:" << std::endl;
				for (const auto& candidate : receivedIceCandidates) {
					std::cout << "Type: " << candidate.type
						<< ", Address: " << candidate.address
						<< ", Port: " << candidate.port
						<< ", Protocol: " << candidate.protocol << std::endl;
				}

				auto clientComm = std::make_shared<ClientCommunication>(receivedIceCandidates, iceCandidates);

				if (!clientComm->connectToClient()) {
					std::cerr << "Failed to connect to client\n";
					return;
				}

				clientComm->startCommunication();
			}
		}

	} catch (const std::exception& e) {
		std::cerr << "Exception in thread: " << e.what() << std::endl;
	}
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <clientId> <targetClientId>" << std::endl;
		return -1;
	}
	std::string clientId = argv[1];
	std::string targetClientId = argv[2];

	std::string nodeUrl = "http://175.45.203.60:8546";
	RPCClient rpcClient(nodeUrl);

	std::string contractAddress = "0xb4DdeFacc0A9C1b13a560714B9F84E6017b0A4C6";

	auto start = std::chrono::high_resolution_clock::now();
	//std::vector<std::string> relays = rpcClient.getRelaysWithRandom(contractAddress, 2);
	std::vector<std::string> relays = rpcClient.getRelays(contractAddress);

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> duration = end - start;
	std::cout << "Blockchain. Time taken: " << duration.count() << " seconds." << std::endl;

	if (relays.empty()) {
		std::cerr << "No relays found." << std::endl;
		return 1;
	}

	start = std::chrono::high_resolution_clock::now();
	// latency 기반으로 상위 count 개 의 릴레이 선택
	std::vector<std::string> selectedRelays = rpcClient.selectLatencyBasedRelays(relays, 1);

	end = std::chrono::high_resolution_clock::now();
	duration = end - start;
	std::cout << "Relay selection time: " << duration.count() << " seconds." << std::endl;

	std::cout << "Selected Relays: ";
	for (const auto& relay : selectedRelays) {
		std::cout << relay << " ";
	}
	std::cout << std::endl;

	// ICE 후보 설정
	ICECandidatesAdapter iceCandidatesAdapter("175.45.203.60", 3478);
	iceCandidatesAdapter.gatherCandidates();
	std::vector<IceCandidate> iceCandidates = iceCandidatesAdapter.getICECandidates();

	std::vector<std::thread> threads;

	for (const auto& relay : selectedRelays) {
		threads.emplace_back(connectToRelay, relay, std::ref(rpcClient), clientId, targetClientId, std::cref(iceCandidates));
	}

	for (auto& thread : threads) {
		thread.join();
	}

	std::cout << "All threads joined. Entering wait loop to keep main function active..." << std::endl;
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	return 0;
}

