// RelayNode.h
#pragma once
#include "ClientManager.h"
#include "NATTraversalHelper.h"
#include "PeerBridgeManager.h"
#include "BlockchainManager.h"

class RelayNode {
public:
    RelayNode();
    // 추가 기능 및 메서드 구현
    
private:
    ClientManager clientManager;
    NATTraversalHelper natTraversalHelper;
    PeerBridgeManager peerBridgeManager;
    BlockchainManager blockchainManager;
};
