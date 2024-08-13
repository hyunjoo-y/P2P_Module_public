#include "PeerBridgeManager.h"
#include <iostream>

RelayNode::RelayNode() {
    m_server.init_asio();

    m_server.set_open_handler(bind(&RelayNode::onOpen, this, std::placeholders::_1));
    m_server.set_close_handler(bind(&RelayNode::onClose, this, std::placeholders::_1));
    m_server.set_message_handler(bind(&RelayNode::onMessage, this, std::placeholders::_1, std::placeholders::_2));
}

void RelayNode::run(uint16_t port) {
    m_server.listen(port);
    m_server.start_accept();
    m_server.run();
}

void RelayNode::onOpen(connection_hdl hdl) {
    // A new connection has been opened, but we don't have a peer ID yet.
    std::cout << "Opened connection" << std::endl;
}

void RelayNode::onClose(connection_hdl hdl) {
    // Remove the connection from the map based on the connection handle.
    for (auto it = m_clientMap.begin(); it != m_clientMap.end(); ++it) {
        if (it->second.lock() == hdl.lock()) {
            std::cout << "Closed connection for peer: " << it->first << std::endl;
            m_clientMap.erase(it);
            break;
        }
    }
}

void RelayNode::onMessage(connection_hdl hdl, WebSocketServer::message_ptr msg) {
    json received_msg = json::parse(msg->get_payload());
    std::string type = received_msg["type"];

    if (type == "register") {
        // Client is registering their peer ID
        std::string nodeAddr = received_msg["nodeAddr"];
        m_clientMap[nodeAddr] = hdl;
        std::cout << "Registered peer: " << nodeAddr << std::endl;
    } else if (type == "connect") {
        // Client wants to connect to another peer
        std::string targetPeerId = received_msg["target"];
        if (m_clientMap.find(targetPeerId) != m_clientMap.end()) {
            // Forward the connect request to the target peer
            json forward_msg;
            forward_msg["type"] = "connect";
            forward_msg["from"] = getConnectionIdentifier(hdl);  // or any identifier representing the source peer
            m_server.send(m_clientMap[targetPeerId], forward_msg.dump(), websocketpp::frame::opcode::text);
        } else {
            std::cout << "Requested peer " << targetPeerId << " not found." << std::endl;
        }
    } else if (type == "ice") {
        // ICE candidate information from one peer to another
        std::string targetPeerId = received_msg["target"];
        if (m_clientMap.find(targetPeerId) != m_clientMap.end()) {
            // Forward the ICE candidates to the target peer
            json forward_msg = received_msg;  // Just forward the entire message
            m_server.send(m_clientMap[targetPeerId], forward_msg.dump(), websocketpp::frame::opcode::text);
        }
    }
    // Add other message types (e.g., 'offer', 'answer') as needed
}

std::string RelayNode::getConnectionIdentifier(connection_hdl hdl) {
    // Unique identifier for this connection; just converting handle to string for simplicity
    return std::to_string(reinterpret_cast<unsigned long long>(hdl.lock().get()));
}

