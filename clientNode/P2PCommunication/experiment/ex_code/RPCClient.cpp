#include "RPCClient.hpp"

#include <iostream>
#include <chrono>
#include <future>
#include <limits>
#include <stdexcept>
#include <cstdlib>
#include <thread>
#include <atomic>

RPCClient::RPCClient(const std::string& nodeUrl) : nodeUrl(nodeUrl) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

RPCClient::~RPCClient() {
    curl_global_cleanup();
}

size_t RPCClient::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string RPCClient::rpcCall(const std::string& method, const json& params) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        json rpcRequest = {
            {"jsonrpc", "2.0"},
            {"method", method},
            {"params", params},
            {"id", 1}
        };

        std::string jsonString = rpcRequest.dump();

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, nodeUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonString.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() 실패: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    } else {
        std::cerr << "curl_easy_init() 실패" << std::endl;
    }

    return readBuffer;
}

std::string RPCClient::hexToAscii(const std::string& hex) {
    std::string ascii;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte = hex.substr(i, 2);
        char chr = static_cast<char>(strtol(byte.c_str(), nullptr, 16));
        if (chr != 0) {
            ascii.push_back(chr);
        }
    }
    return ascii;
}

std::string RPCClient::removeNonPrintable(const std::string& str) {
    std::string result;
    for (char c : str) {
        if (isprint(static_cast<unsigned char>(c))) {
            result += c;
        }
    }
    return result;
}

std::string RPCClient::trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

std::string RPCClient::removeSpaces(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        if (!std::isspace(c)) {
            result.push_back(c);
        }
    }
    return result;
}

std::pair<std::string, int> RPCClient::getIpPort(const std::string& url) {
    std::regex ipPortPattern("http://([0-9.]+):([0-9]+)");
    std::smatch match;
    if (std::regex_search(url, match, ipPortPattern)) {
        return {match[1].str(), std::stoi(match[2].str())};
    }
    return {"", 0};
}

std::string RPCClient::uint256ToHex(boost::multiprecision::uint256_t value) {
    std::stringstream ss;
    ss << std::hex << value;
    std::string hexStr = ss.str();
    // Ensure the hex string has 64 characters (32 bytes)
    if (hexStr.length() < 64) {
        hexStr = std::string(64 - hexStr.length(), '0') + hexStr;
    }
    return hexStr;
}

double RPCClient::measureLatency(const std::string& ip, int port) {
    std::string cmd = "echo quit | telnet " + ip + " " + std::to_string(port) + " 2>&1 | grep 'Connected'";

    auto start = std::chrono::high_resolution_clock::now();

    // Run the command asynchronously
    auto future = std::async(std::launch::async, [cmd]() -> std::string {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            return "";
        }

        char buffer[128];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
        return result;
    });

    // Set a timeout period (e.g., 5 seconds)
    std::chrono::seconds timeout(3);

    if (future.wait_for(timeout) == std::future_status::timeout) {
        std::cerr << "Connection timed out for IP: " << ip << " Port: " << port << std::endl;
        return 0;
    }

    std::string result = future.get();
    auto end = std::chrono::high_resolution_clock::now();

    if (result.find("Connected") == std::string::npos) {
        std::cerr << "Connection failed for IP: " << ip << " Port: " << port << std::endl;
        return 0;
    }

    std::chrono::duration<double, std::milli> elapsed = end - start;
    return elapsed.count();
}


void RPCClient::measureNodeLatency(const std::string& node, std::vector<RelayConnection>& results) {
    std::pair<std::string, int> ipPort = getIpPort(node);
    std::string ipAddress = ipPort.first;
    int port = ipPort.second;

    if (ipAddress.empty() || port == 0) return;

    double latency = measureLatency(ipAddress, port);
    std::lock_guard<std::mutex> lock(latencyMutex);
    std::cout << "Node: " << ipAddress << " Port: " << port << " Latency: " << latency << " ms" << std::endl;
    results.push_back({node, latency});
}

std::vector<std::string> RPCClient::selectLatencyBasedRelays(const std::vector<std::string>& relays, size_t count) {
    std::vector<std::string> selectedRelays;
    std::vector<std::thread> threads;
    std::atomic<bool> stopFlag(false);

    for (const auto& relay : relays) {
        threads.emplace_back([relay, &selectedRelays, &stopFlag, &count, this]() {
            double latency = pingRelay(relay);
            if (latency != std::numeric_limits<double>::max()) {
                std::lock_guard<std::mutex> lock(latencyMutex);
                if (selectedRelays.size() < count && !stopFlag.load()) {
                    selectedRelays.push_back(relay);
                    std::cout << "Relay selected: " << relay << " (Latency: " << latency << " ms)" << std::endl;
                    if (selectedRelays.size() == count) {
                        stopFlag.store(true);  // 필요한 릴레이 수를 모두 찾았으므로 종료
                        cv.notify_all();
                    }
                }
            }
        });
    }

    {
        std::unique_lock<std::mutex> lock(latencyMutex);
        cv.wait(lock, [&selectedRelays, &count]() { return selectedRelays.size() >= count; });
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.detach();  // 스레드를 분리하여 빠르게 반환
        }
    }

    return selectedRelays;
}


/*std::vector<std::string> RPCClient::selectLatencyBasedRelays(const std::vector<std::string>& relays, size_t count) {

	std::cout << "COUNT" << std::endl;
	std::cout << "count: " << count <<std::endl;
    std::vector<std::promise<double>> promises(relays.size());
    std::vector<std::future<double>> futures;
    for (auto& promise : promises) {
        futures.push_back(promise.get_future());
    }
    std::atomic<size_t> completedCount(0);
    std::priority_queue<std::pair<double, std::string>, std::vector<std::pair<double, std::string>>, std::greater<>> relayQueue;
    std::mutex latencyMutex;
    std::condition_variable cv;
    std::atomic<bool> stopProcessing(false); // 스레드 중지 플래그

    auto worker = [&](const std::string& relay, std::promise<double>& promise) {
        std::cout << "Starting ping for relay: " << relay << std::endl;
        double latency = pingRelay(relay);
        std::cout << "Ping complete for relay: " << relay << " with latency: " << latency << std::endl;
        if (latency != std::numeric_limits<double>::max()) {
            {
                std::lock_guard<std::mutex> lock(latencyMutex);
                if (stopProcessing.load()) {
                    std::cout << "Stopping processing for relay: " << relay << std::endl;
                    return; // 중지 플래그 확인
                }
                relayQueue.emplace(latency, relay);
                std::cout << "Relay added to queue: " << relay << " with latency: " << latency << std::endl;
                if (++completedCount >= count) {
                    stopProcessing.store(true); // 필요한 수의 릴레이가 선택되면 중지 플래그 설정
                    cv.notify_all();
                    std::cout << "Relay selection complete. Count reached: " << completedCount << std::endl;
                }
            }
            promise.set_value(latency);
        } else {
            promise.set_value(std::numeric_limits<double>::max());
        }
    };

    std::vector<std::thread> threads;
    for (size_t i = 0; i < relays.size(); ++i) {
        threads.emplace_back(worker, relays[i], std::ref(promises[i]));
    }

    // 상위 count 개의 릴레이가 완료될 때까지 대기
    {
        std::unique_lock<std::mutex> lock(latencyMutex);
        cv.wait(lock, [&completedCount, count]() { return completedCount >= count; });
    }

    // 스레드 종료
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    // 상위 count 개의 릴레이 선택
    std::vector<std::string> selectedRelays;
    {
        std::lock_guard<std::mutex> lock(latencyMutex);
        for (size_t i = 0; i < count && !relayQueue.empty(); ++i) {
            selectedRelays.push_back(relayQueue.top().second);
            relayQueue.pop();
        }
    }

    return selectedRelays;
}*/

// manageRelayConnections 함수 추가
void RPCClient::manageRelayConnections() {
    for (const auto& connection : relayConnections) {
        std::cout << "Managing connection to relay: " << connection.relay << " with latency: " << connection.latency << " ms" << std::endl;
    }
}

std::vector<std::string> RPCClient::getRelays(const std::string& contractAddress) {
    std::vector<std::string> relays;
    std::string functionSelector = "0x23a93b1e";
    json callObject = {
        {"to", contractAddress},
        {"data", functionSelector}
    };
    json params = {callObject, "latest"};
    std::string response = rpcCall("eth_call", params);

    try {
        auto jsonResponse = json::parse(response);
        if (jsonResponse.contains("result")) {
            std::string result = jsonResponse["result"].get<std::string>().substr(2);
            std::cout << "Raw relay data: " << result << std::endl;

            std::string relay = hexToAscii(result);
            relay.erase(std::remove(relay.begin(), relay.end(), '\0'), relay.end());
            relay = trim(relay);
            relay = removeSpaces(relay);
            relay = removeNonPrintable(relay);
            std::cout << "Cleaned relay: " << relay << std::endl;

            std::regex urlPattern("http://[0-9.]+:[0-9]+");
            std::sregex_iterator iter(relay.begin(), relay.end(), urlPattern);
            std::sregex_iterator end;
            while (iter != end) {
                relays.push_back(iter->str());
                ++iter;
            }
        } else if (jsonResponse.contains("error")) {
            std::cerr << "JSON-RPC 오류: " << jsonResponse["error"]["message"] << std::endl;
        } else {
            std::cerr << "JSON 응답에 'result'가 없음: " << jsonResponse.dump() << std::endl;
        }
    } catch (const json::parse_error& e) {
        std::cerr << "JSON 파싱 오류: " << e.what() << std::endl;
    } catch (const std::out_of_range& e) {
        std::cerr << "문자열 파싱 오류: " << e.what() << std::endl;
    }

    return relays;
}

std::vector<std::string> RPCClient::getRelaysWithRandom(const std::string& contractAddress, boost::multiprecision::uint256_t count) {
    std::vector<std::string> relays;
    std::string functionSelector = "0x0f4cf7f9";

    std::string countHex = uint256ToHex(count);

    std::string data = functionSelector + countHex;

    nlohmann::json callObject = {
        {"to", contractAddress},
        {"data", data},
    };


    json params = {callObject, "latest"};
    std::string response = rpcCall("eth_call", params);

    try {
        auto jsonResponse = json::parse(response);
        if (jsonResponse.contains("result")) {
            std::string result = jsonResponse["result"].get<std::string>().substr(2);
            std::cout << "Raw relay data: " << result << std::endl;

            std::string relay = hexToAscii(result);
            relay.erase(std::remove(relay.begin(), relay.end(), '\0'), relay.end());
            relay = trim(relay);
            relay = removeSpaces(relay);
            relay = removeNonPrintable(relay);
            std::cout << "Cleaned relay: " << relay << std::endl;

            std::regex urlPattern("http://[0-9.]+:[0-9]+");
            std::sregex_iterator iter(relay.begin(), relay.end(), urlPattern);
            std::sregex_iterator end;
            while (iter != end) {
                relays.push_back(iter->str());
                ++iter;
            }
        } else if (jsonResponse.contains("error")) {
            std::cerr << "JSON-RPC 오류: " << jsonResponse["error"]["message"] << std::endl;
        } else {
            std::cerr << "JSON 응답에 'result'가 없음: " << jsonResponse.dump() << std::endl;
        }
    } catch (const json::parse_error& e) {
        std::cerr << "JSON 파싱 오류: " << e.what() << std::endl;
    } catch (const std::out_of_range& e) {
        std::cerr << "문자열 파싱 오류: " << e.what() << std::endl;
    }

    return relays;
}

double RPCClient::pingRelay(const std::string& relay) {
    std::pair<std::string, int> ipPort = getIpPort(relay);
    std::string ipAddress = ipPort.first;
    int port = ipPort.second;

    if (ipAddress.empty() || port == 0) return std::numeric_limits<double>::max();

    double latency = measureLatency(ipAddress, port);
    if (latency < 0) {
        return std::numeric_limits<double>::max();
    }
    return latency;
}
