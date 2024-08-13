#ifndef KADEMLIA_NODE_H
#define KADEMLIA_NODE_H

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

class KademliaNode {
public:
    std::string node_id;
    std::string ip;
    int port;
    std::unordered_map<std::string, std::string> storage;
    std::vector<std::pair<std::string, int>> peers; // (IP, port) pairs
    std::mutex mtx;

    KademliaNode(const std::string &id, const std::string &ip, int port);

    void store(const std::string &key, const std::string &value);
    std::string find_value(const std::string &key);
    void add_peer(const std::string &peer_ip, int peer_port);
    void start_server();
    void join_network(const std::string &bootstrap_ip, int bootstrap_port);
    void print_peers(); // 새로운 함수

private:
    static const int K = 20; // Kademlia k-bucket size
    void handle_client(int socket);
    void send_peers(const std::string &new_node_ip, int new_node_port);
    std::string send_find_request(const std::string &peer_ip, int peer_port, const std::string &key);
    std::string send_message(const std::string &peer_ip, int peer_port, const std::string &message);
    void process_peer_message(const std::string &message);
    void update_peers(const std::string &peer_ip, int peer_port);
    static int xor_distance(const std::string &id1, const std::string &id2);
};

std::string sha1(const std::string &input);

#endif // KADEMLIA_NODE_H

