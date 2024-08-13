#include "RPCClient.hpp"
#include "RelayCommunication.hpp"
#include "ClientCommunication.hpp"
#include <atomic>
#include <iostream>
#include <chrono>
#include <functional>
#include <future>
#include <queue>

//std::vector<IceCandidate> receivedIceCandidates;
std::vector<IceCandidate> iceCandidates;

std::atomic<bool> finished(false);
std::atomic<bool> relayInfoDone(false);
std::atomic<bool> receivedIceCandidatesDone(false);
std::condition_variable comm_cv;

std::atomic<bool> relayConnected(false);
std::atomic<bool> globalIceCandidatesSet(false);
std::mutex commMutex;
std::vector<IceCandidate> globalReceivedIceCandidates;
std::pair<std::string, int> iceRelayInfo;



void getIceRelayInfoThread(RelayCommunication* relayComm, std::atomic<bool>& relayInfoDone, std::atomic<bool>& finished, std::pair<std::string, int>& relay_info) {
    while (!finished.load()) {
        auto info = relayComm->getIceRelayInfo();
        {
            std::lock_guard<std::mutex> lock(commMutex);
            relay_info = info;
        }
        relayInfoDone.store(true);
	std::cout << "Relay info thread: Info received\n";
    }
}

void getReceivedIceCandidatesThread(RelayCommunication* relayComm, std::atomic<bool>& receivedIceCandidatesDone, std::atomic<bool>& finished, std::vector<IceCandidate>& receivedIceCandidates) {
    while (!finished.load()) {
        auto candidates = relayComm->getReceivedIceCandidates();
        {
            std::lock_guard<std::mutex> lock(commMutex);
            receivedIceCandidates = candidates;
        }
        receivedIceCandidatesDone.store(true);
	std::cout << "Received ICE candidates thread: Candidates received\n";
    //    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 잠시 대기하여 busy-waiting 방지
    }
}

void relayCommunicationThread(RelayCommunication* relayComm, const std::string& targetClientId) {
    if (relayComm->connectToRelay()) {
        std::cout << "Successfully connected to relay2 " << std::endl;
        relayComm->startCommunication(targetClientId);

        while (true) {
		auto receivedIceCandidates = relayComm->getReceivedIceCandidates();
		if (!receivedIceCandidates.empty()) {
			for (const auto& candidate : receivedIceCandidates) {
				std::cout << "Type: " << candidate.type
					<< ", Address: " << candidate.address
					<< ", Port: " << candidate.port
					<< ", Protocol: " << candidate.protocol << std::endl;
			}

			{
	//			std::lock_guard<std::mutex> lock(commMutex);
				globalReceivedIceCandidates = receivedIceCandidates;
				globalIceCandidatesSet.store(true);
				relayConnected.store(true);
				std::cout << "Successfully saved" << std::endl;
				comm_cv.notify_all(); // 상태 변화 알림
			}
			
			break; // 후보를 받았으므로 루프를 종료
		}
        }
    }
}

void relayFileSharing(RelayCommunication* relayComm, const std::string& targetClientId) {
    if (relayComm->connectToRelay()) {
        std::cout << "Successfully connected to relay(File Sharing) " << std::endl;
	    relayComm->startCommunication(targetClientId);

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
	            relayComm->sendFileToRelay(filename, targetClientId);
	        }else if (strcmp(input, "exit") == 0){
			return;
		}
        }
    }
}


void connectToRelay(const std::string& relay, RPCClient& rpcClient, const std::string& clientId, const std::string& targetClientId, const std::vector<IceCandidate>& iceCandidates) {
    try {
        if (globalIceCandidatesSet.load()) {
            return;
        }

        std::pair<std::string, int> ipPort = rpcClient.getIpPort(relay);
        std::string relayIp = ipPort.first;
        int relayPort = ipPort.second;

        std::unique_ptr<RelayCommunication> relayComm;

        try {
            relayComm = std::make_unique<RelayCommunication>(relayIp, relayPort, clientId, iceCandidates);
            std::cout << "RelayCommunication object created successfully." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to create RelayCommunication object: " << e.what() << std::endl;
            return;
	}

	if (relayComm->connectToRelay()) {
		std::cout << "Successfully connected to relay: " << relay << std::endl;
		relayComm->startCommunication(targetClientId);

		std::pair<std::string, int> relay_info;
		std::vector<IceCandidate> receivedIceCandidates;

		// Separate threads to get relay info and ice candidates
		std::thread relayInfoThread(getIceRelayInfoThread, relayComm.get(), std::ref(relayInfoDone), std::ref(finished), std::ref(relay_info));
		std::thread receivedIceCandidatesThread(getReceivedIceCandidatesThread, relayComm.get(), std::ref(receivedIceCandidatesDone), std::ref(finished), std::ref(receivedIceCandidates));

		

		auto start = std::chrono::high_resolution_clock::now();
		
		relayInfoThread.detach();
		receivedIceCandidatesThread.detach();

		while (true) {
			if (relayInfoDone.load()) {
				{
					std::lock_guard<std::mutex> lock(commMutex);
					if (!relay_info.first.empty() && relay_info.second != 0) {
						std::cout << "RELAY INFO from " << relay << ": " << std::endl;
						std::cout << "Stored IP Address: " << relay_info.first << ", Port: " << relay_info.second << std::endl;
						iceRelayInfo = relay_info;

						 std::unique_ptr<RelayCommunication> relayComm2;
						 try {
							 relayComm2 = std::make_unique<RelayCommunication>(relay_info.first, relay_info.second, clientId, iceCandidates);
							 std::cout << "RelayCommunication object created successfully." << std::endl;
						 } catch (const std::exception& e) {
							 std::cerr << "Failed to create RelayCommunication object: " << e.what() << std::endl;
							 return;
						 }
						// Relay2와의 새로운 스레드를 생성하여 연결 설정
						std::thread relay2Thread(relayCommunicationThread, relayComm2.get(), targetClientId);
						relay2Thread.join();
						auto end = std::chrono::high_resolution_clock::now();
						std::chrono::duration<double> duration = end - start;
						std::cout << "ICE Exchanged through new Relay. Time taken: " << duration.count() << " seconds." << std::endl;

						// relay_info 사용 후 비움
						relayComm->setIceRelayInfo("", 0);
						finished.store(true);
						break;
					} 
				}
			}

			if (receivedIceCandidatesDone.load() && !relayConnected.load()) {
				{
					std::lock_guard<std::mutex> lock(commMutex);
					if (!receivedIceCandidates.empty()) {
						auto end = std::chrono::high_resolution_clock::now();
						std::chrono::duration<double> duration = end - start;
						std::cout << "ICE Exchanged through existing Relay. Time taken: " << duration.count() << " seconds." << std::endl;
						
						std::cout << "ICE Candidates from relay " << relay << ":" << std::endl;
						for (const auto& candidate : receivedIceCandidates) {
							std::cout << "Type: " << candidate.type
								<< ", Address: " << candidate.address
								<< ", Port: " << candidate.port
								<< ", Protocol: " << candidate.protocol << std::endl;
						}

						globalReceivedIceCandidates = receivedIceCandidates;
						globalIceCandidatesSet.store(true);
						relayConnected.store(true);

						finished.store(true);
						break; // 후보를 받았으므로 루프를 종료
					}
				}
			}

		}
		while (true) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

    } catch (const std::exception& e) {
	    std::cerr << "Exception in thread: " << e.what() << std::endl;
    }
}

void connectToClient(const std::vector<IceCandidate>& receivedIceCandidates, const std::vector<IceCandidate>& iceCandidates, const std::string& clientId, const std::string& targetClientId) {
    try {
        std::unique_ptr<ClientCommunication> clientComm;

        try {
            clientComm = std::make_unique<ClientCommunication>(receivedIceCandidates, iceCandidates);
            std::cout << "ClientCommunication object created successfully." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to create ClientCommunication object: " << e.what() << std::endl;
            return;
        }

        if (!clientComm->connectToClient()) {
            	std::cerr << "Failed to connect to client\n";
		// 실패하면 릴레이 통해 데이터 전송
		std::unique_ptr<RelayCommunication> relayComm;
		try {
			relayComm = std::make_unique<RelayCommunication>(iceRelayInfo.first, iceRelayInfo.second, clientId, iceCandidates);
			std::cout << "RelayCommunication object created successfully." << std::endl;
		} catch (const std::exception& e) {
			std::cerr << "Failed to create RelayCommunication object: " << e.what() << std::endl;
			return;
		}
		
		std::thread relayFileThread(relayFileSharing, relayComm.get(), targetClientId);
		relayFileThread.detach();
        } else {
		clientComm->startCommunication();
		if(globalIceCandidatesSet.load()){
			while (true) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}  
		}

        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in client connection: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <clientId> <targetClientId> <selected_relay>" << std::endl;
        return -1;
    }

    std::string clientId = argv[1];
    std::string targetClientId = argv[2];
    int relay_num = std::stoi(argv[3]);

    std::string nodeUrl = "http://175.45.203.60:8546";
    RPCClient rpcClient(nodeUrl);

    std::string contractAddress = "0xb207Cde8Af120f968F618C3E6456C8a4A9d3a869";
    auto start = std::chrono::high_resolution_clock::now();
    //std::vector<std::string> relays = rpcClient.getRelaysWithRandom(contractAddress, relay_num);
    std::vector<std::string> relays = rpcClient.getRelays(contractAddress);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "Blockchain. Time taken: " << duration.count() << " seconds." << std::endl;

    if (relays.empty()) {
        std::cerr << "No relays found." << std::endl;
        return 1;
    }

    start = std::chrono::high_resolution_clock::now();
    // latency 기반으로 상위 n개의 릴레이 선택
    std::vector<std::string> selectedRelays = rpcClient.selectLatencyBasedRelays(relays, relay_num);
    //std::vector<std::string> selectedRelays = relays;
	    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    std::cout << "Relay selection time: " << duration.count() << " seconds." << std::endl;
	std::pair<std::string, int> stunIpPort;
bool stunIP = false;

    std::cout << "Selected Relays: ";
    for (const auto& relay : selectedRelays) {
        std::cout << relay << " ";
	    if(!stunIP){
	  stunIpPort   = rpcClient.getIpPort(relay);
		    stunIP = true;
	    }
    }
    std::cout << std::endl;

    // ICE 후보 설정
    ICECandidatesAdapter iceCandidatesAdapter(stunIpPort.first, 3478);
    iceCandidatesAdapter.gatherCandidates();
    std::vector<IceCandidate> iceCandidates = iceCandidatesAdapter.getICECandidates();
    std::vector<std::thread> threads;


	  start = std::chrono::high_resolution_clock::now();
    for (const auto& relay : selectedRelays) {
        if (!globalIceCandidatesSet.load()) {
            threads.emplace_back(connectToRelay, relay, std::ref(rpcClient), clientId, targetClientId, std::cref(iceCandidates));
        } else {
            break;
        }
    }

    for (auto& thread : threads) {
        thread.detach();
    }


    // 주 스레드는 첫 번째 연결이 성공하고 ICE 후보가 설정될 때까지 대기
    while (!globalIceCandidatesSet.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    std::cout << "ICE Exchanged through  " << relay_num << " Relays. Time taken: " << duration.count() << " seconds." << std::endl;

    // 클라이언트와의 연결 시작
    connectToClient(globalReceivedIceCandidates, iceCandidates, clientId, targetClientId);
    
    while (true) {
	    std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}

