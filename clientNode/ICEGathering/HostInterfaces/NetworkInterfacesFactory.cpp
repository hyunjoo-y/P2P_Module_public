#include "NetworkInterfacesFactory.h"
#include "NetworkInterfacesUnix.h"
#include "NetworkInterfacesWindows.h"
#include <memory>

std::unique_ptr<NetworkInterfaces> createNetworkInterfaces() {
#ifdef _WIN32
    return std::unique_ptr<NetworkInterfaces>(new NetworkInterfacesWindows());
#else
    return std::unique_ptr<NetworkInterfaces>(new NetworkInterfacesUnix());
#endif
}

