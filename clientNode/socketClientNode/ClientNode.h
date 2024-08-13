#ifndef CLIENT_NODE_H
#define CLIENT_NODE_H
#include "./ICEGathering/ICECandidatesAdapter.h"

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <string>

typedef websocketpp::client<websocketpp::config::asio_client> WSClient;
using websocketpp::connection_hdl;
using json = nlohmann::json;

class ClientNode {
public:
    ClientNode(const std::string& relayURI,const std::string& stunServer, uint16_t stunPort);
    void connectToRelay(const std::string& nodeAddr);
    void requestConnection(const std::string& peerId);
    void sendICECandidates();

private:
    client m_client;
    std::string m_relayURI;
    std::string m_nodeAddr; // Identifier for this client
    ICECandidatesAdapter m_iceAdapter;
    connection_hdl m_serverConnectionHandle;

    void onOpen(connection_hdl hdl);
    void onMessage(connection_hdl hdl, client::message_ptr msg);
    void onClose(connection_hdl hdl);
    void handleICECandidate(const json& candidate, const std::string& sender);
    void registerToServer();
};

#endif // CLIENT_NODE_H
