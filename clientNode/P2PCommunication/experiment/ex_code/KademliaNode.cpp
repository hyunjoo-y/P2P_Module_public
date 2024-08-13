#include "KademliaNode.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <iomanip>
#include <algorithm>

// 해시 함수 정의
std::string sha1(const std::string &input) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

    std::ostringstream os;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        os << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return os.str();
}

KademliaNode::KademliaNode(const std::string &id, const std::string &ip, int port)
    : node_id(id), ip(ip), port(port) {
    std::cout << "Node created with ID: " << node_id << ", IP: " << ip << ", Port: " << port << std::endl;
}

void KademliaNode::store(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> lock(mtx);
    storage[key] = value;
    std::cout << "Stored key: " << key << " with value: " << value << std::endl;
}

std::string KademliaNode::find_value(const std::string &key) {
    std::lock_guard<std::mutex> lock(mtx);
    if (storage.find(key) != storage.end()) {
        std::cout << "Found value for key: " << key << " locally." << std::endl;
        return ip + ":" + std::to_string(port);
    } else {
        std::cout << "Key: " << key << " not found in local storage. Querying peers..." << std::endl;
        for (const auto &peer : peers) {
            std::string value = send_find_request(peer.first, peer.second, key);
            if (!value.empty()) {
                return value;
            }
        }
    }
    return "";
}

void KademliaNode::add_peer(const std::string &peer_ip, int peer_port) {
    // 자신을 피어 목록에 추가하지 않음
    if (peer_ip == ip && peer_port == port) {
        return;
    }
    update_peers(peer_ip, peer_port);
    std::cout << "Added peer: " << peer_ip << ":" << peer_port << std::endl;
}

void KademliaNode::update_peers(const std::string &peer_ip, int peer_port) {
    std::lock_guard<std::mutex> lock(mtx);
    peers.push_back(std::make_pair(peer_ip, peer_port));
    // 가장 가까운 K개의 피어만 유지
    std::sort(peers.begin(), peers.end(), [this](const std::pair<std::string, int> &a, const std::pair<std::string, int> &b) {
        return xor_distance(node_id, sha1(a.first + ":" + std::to_string(a.second))) <
               xor_distance(node_id, sha1(b.first + ":" + std::to_string(b.second)));
    });
    if (peers.size() > K) {
        peers.resize(K);
    }
}

int KademliaNode::xor_distance(const std::string &id1, const std::string &id2) {
    std::string::size_type len = std::min(id1.size(), id2.size());
    int distance = 0;
    for (std::string::size_type i = 0; i < len; ++i) {
        distance = (distance << 8) + (id1[i] ^ id2[i]);
    }
    return distance;
}

void KademliaNode::start_server() {
    std::thread([this]() {
        int server_fd, new_socket;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);

        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }
        if (listen(server_fd, 3) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        std::cout << "Server started on port: " << port << std::endl;

        while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0) {
            std::cout << "Accepted connection" << std::endl;
            std::thread(&KademliaNode::handle_client, this, new_socket).detach();
        }
    }).detach();
}

void KademliaNode::join_network(const std::string &bootstrap_ip, int bootstrap_port) {
    std::string message = "JOIN " + ip + " " + std::to_string(port);
    std::string response = send_message(bootstrap_ip, bootstrap_port, message);
    std::cout << "Joined network with bootstrap node: " << bootstrap_ip << ":" << bootstrap_port << std::endl;
    std::cout << "Response from bootstrap: " << response << std::endl;

    // 부트스트랩 노드로부터 받은 피어 정보를 처리
    std::istringstream iss(response);
    std::string peer_ip;
    int peer_port;
    while (iss >> peer_ip >> peer_port) {
        add_peer(peer_ip, peer_port);
    }

    // 피어 탐색 시작
    for (const auto &peer : peers) {
        std::string peer_response = send_find_request(peer.first, peer.second, node_id);
        std::istringstream peer_iss(peer_response);
        while (peer_iss >> peer_ip >> peer_port) {
            add_peer(peer_ip, peer_port);
        }
    }
}

void KademliaNode::handle_client(int socket) {
    char buffer[1024] = {0};
    read(socket, buffer, 1024);
    std::string request(buffer);
    std::cout << "Received request: " << request << std::endl;

    if (request.substr(0, 8) == "FIND_KEY") {
        std::string key = request.substr(9);
        std::string value = find_value(key);
        send(socket, value.c_str(), value.size(), 0);
    } else if (request.substr(0, 5) == "STORE") {
        std::istringstream iss(request.substr(6));
        std::string key, value;
        iss >> key >> value;
        store(key, value);
    } else if (request.substr(0, 4) == "JOIN") {
        std::istringstream iss(request.substr(5));
        std::string new_node_ip;
        int new_node_port;
        iss >> new_node_ip >> new_node_port;
        add_peer(new_node_ip, new_node_port);
        send_peers(new_node_ip, new_node_port);
    } else if (request.substr(0, 4) == "PEER") {
        process_peer_message(request);
    }
    close(socket);
}

void KademliaNode::send_peers(const std::string &new_node_ip, int new_node_port) {
    std::cout << "Sending peer information to new node: " << new_node_ip << ":" << new_node_port << std::endl;
    for (const auto &peer : peers) {
        std::string message = "PEER " + peer.first + " " + std::to_string(peer.second);
        std::string response = send_message(new_node_ip, new_node_port, message);
        std::cout << "Sent PEER message to " << new_node_ip << ":" << new_node_port << " with response: " << response << std::endl;
    }
}

std::string KademliaNode::send_find_request(const std::string &peer_ip, int peer_port, const std::string &key) {
    return send_message(peer_ip, peer_port, "FIND_KEY " + key);
}

std::string KademliaNode::send_message(const std::string &peer_ip, int peer_port, const std::string &message) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return "";
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(peer_port);

    if (inet_pton(AF_INET, peer_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return "";
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return "";
    }

    send(sock, message.c_str(), message.size(), 0);
    read(sock, buffer, 1024);
    close(sock);

    std::cout << "Sent message: " << message << " to " << peer_ip << ":" << peer_port << std::endl;
    std::cout << "Received response: " << std::string(buffer) << std::endl;

    return std::string(buffer);
}

void KademliaNode::process_peer_message(const std::string &message) {
    std::istringstream iss(message.substr(5));
    std::string peer_ip;
    int peer_port;
    iss >> peer_ip >> peer_port;
    add_peer(peer_ip, peer_port);
    std::cout << "Processed PEER message: " << message << std::endl;
}

void KademliaNode::print_peers() {
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "Current peers:" << std::endl;
    for (const auto &peer : peers) {
        std::cout << "Peer IP: " << peer.first << ", Port: " << peer.second << std::endl;
    }
}

