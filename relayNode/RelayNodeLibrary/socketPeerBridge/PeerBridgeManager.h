#ifndef RELAY_NODE_H
#define RELAY_NODE_H

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <unordered_map>
#include <string>

typedef websocketpp::server<websocketpp::config::asio> WWSServer;
using websocketpp::connection_hdl;
using json = nlohmann::json;

class RelayNode {
public:
    RelayNode();
    void run(uint16_t port);

private:
    WWSServer m_server;
    std::unordered_map<std::string, connection_hdl> m_clientMap; // Maps a peer ID to a connection handle

    void onOpen(connection_hdl hdl);
    void onClose(connection_hdl hdl);
    void onMessage(connection_hdl hdl, WebSocketServer::message_ptr msg);
    std::string getConnectionIdentifier(connection_hdl hdl);
};

#endif // RELAY_NODE_H

