#ifndef RPC_CLIENT_HPP
#define RPC_CLIENT_HPP

#include <string>
#include <vector>
#include <curl/curl.h>
#include <regex>
#include <mutex>
#include <thread>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <queue>
#include "json.hpp"
#include <boost/multiprecision/cpp_int.hpp>
#include <condition_variable>

using json = nlohmann::json;

struct RelayConnection {
    std::string relay;
    double latency;
    std::thread connectionThread;

    RelayConnection(const std::string& relay, double latency)
        : relay(relay), latency(latency) {}
};

class RPCClient {
public:
    RPCClient(const std::string& nodeUrl);
    ~RPCClient();

    std::pair<std::string, int> getIpPort(const std::string& url);
    std::vector<std::string> getRelays(const std::string& contractAddress);
    std::vector<std::string> selectLatencyBasedRelays(const std::vector<std::string>& relays, size_t count);
    std::vector<std::string> getRelaysWithRandom(const std::string& contractAddress, boost::multiprecision::uint256_t count);
    void manageRelayConnections();

private:
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    std::string rpcCall(const std::string& method, const json& params);
    std::string hexToAscii(const std::string& hex);
    double pingRelay(const std::string& relay);
    std::string removeNonPrintable(const std::string& str);
    std::string trim(const std::string& str);
    std::string removeSpaces(const std::string& str);
    std::string uint256ToHex(boost::multiprecision::uint256_t value);
    double measureLatency(const std::string& ip, int port);
    void measureNodeLatency(const std::string& node, std::vector<RelayConnection>& results);
    std::string selectLatencyBasedNode(const std::vector<std::string>& nodes);

    std::string nodeUrl;
    std::mutex latencyMutex;
    std::vector<RelayConnection> relayConnections;
    std::condition_variable cv;
};

#endif // RPC_CLIENT_HPP
