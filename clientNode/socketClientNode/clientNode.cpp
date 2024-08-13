#include "ClientNode.h"

#include <iostream>

ClientNode::ClientNode(const std::string& relayURI, const std::string& stunServer, uint16_t stunPort) 
    : m_serverURI(relayURI), m_iceAdapter(stunServer, stunPort) {
    m_client.init_asio();
    m_client.set_open_handler(bind(&ClientNode::onOpen, this, ::_1));
    m_client.set_message_handler(bind(&ClientNode::onMessage, this, ::_1, ::_2));
    m_client.set_close_handler(bind(&ClientNode::onClose, this, ::_1));
}

void ClientNode::connectToRelay(const std::string& nodeAddr) {
    m_nodeAddr = nodeAddr;
    websocketpp::lib::error_code ec;
    client::connection_ptr con = m_client.get_connection(m_serverURI, ec);
    if (ec) {
        std::cout << "Could not create connection because: " << ec.message() << std::endl;
        return;
    }
    m_serverConnectionHandle = con->get_handle();
    m_client.connect(con);
    m_client.run();
}

void ClientNode::requestConnection(const std::string& nodeAddr) {
    m_nodeAddr = nodeAddr;
    json message;
    message["type"] = "connect";
    message["target"] = nodeAddr;
    m_client.send(m_serverConnectionHandle, message.dump(), websocketpp::frame::opcode::text);
}

void ClientNode::sendICECandidates() {
    m_iceAdapter.gatherCandidates(); // This should be asynchronous or in a separate thread
    const auto& candidates = m_iceAdapter.getICECandidates();

    for (const auto& candidate : candidates) {
        json message;
        message["type"] = "ice";
        message["candidate"] = {{"type", candidate.type}, {"address", candidate.address}, {"port", candidate.port}, {"protocol", candidate.protocol}};
        message["target"] = m_peerId;
        m_client.send(m_serverConnectionHandle, message.dump(), websocketpp::frame::opcode::text);
    }
}

void ClientNode::onOpen(connection_hdl hdl) {
    std::cout << "Connected to server." << std::endl;
    // The connection to the signaling server is now open, could send some initialization data here if necessary
    registerToServer();
}

void ClientNode::registerToServer() {
    json message;
    message["type"] = "register";
    message["nodeAddr"] = m_nodeAddr; // 이 노드의 고유 Peer ID를 서버에 보냅니다.
    m_client.send(m_serverConnectionHandle, message.dump(), websocketpp::frame::opcode::text);
}

void ClientNode::onMessage(connection_hdl hdl, WSClient::message_ptr msg) {
    std::cout << "Received message: " << msg->get_payload() << std::endl;
    processSignalingMessage(msg->get_payload());
}

void ClientNode::onClose(connection_hdl hdl) {
    std::cout << "Disconnected." << std::endl;
}

void ClientNode::processSignalingMessage(const std::string& message) {
    // Parse the incoming message and react to it depending on its type
    // This should include reacting to 'connect', 'ice', 'offer', 'answer' messages
    auto jMessage = json::parse(message);
    std::string type = jMessage["type"];

    if (type == "ice") {
        // Process incoming ICE candidates from the remote peer
        std::cout << "Received ICE candidate from peer." << std::endl;
        // Here you would typically add this candidate to your WebRTC peer connection
    }
    // Add more message processing as needed based on your signaling protocol
}
