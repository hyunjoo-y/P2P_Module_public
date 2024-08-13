#ifndef NAT_TRAVERSAL_HELPER_H
#define NAT_TRAVERSAL_HELPER_H

#include <string>
#include <utility>
#include <cstdint>

struct STUNConfig {
    std::string address;
    uint16_t port;
    uint16_t min_port;
    uint16_t max_port;
};

class NATTraversalHelper {
public:
    NATTraversalHelper();
    ~NATTraversalHelper();
    static bool installCoturnServer();
    static bool startCoturnServer();
    static bool stopCoturnServer();
    static bool checkCoturnServerStatus();
    static std::pair<std::string, int> getCoturnServerAddress();
    static bool setConfig(uint16_t port, uint16_t min_port, uint16_t max_port);

private:
    static STUNConfig stunConfig_;
    
    static std::string execCommand(const char* cmd);
    static std::string getOperatingSystem();
    static bool writeConfigFile(const std::string& osType);
};

#endif // NAT_TRAVERSAL_HELPER_H

