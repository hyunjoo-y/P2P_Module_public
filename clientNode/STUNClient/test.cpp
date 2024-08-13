#include "NetworkInterfacesFactory.h"

int main() {
    auto networkInterfaces = createNetworkInterfaces();
    auto addresses = networkInterfaces->getLocalIPAddresses();

    for (const auto& address : addresses) {
        std::cout << address << std::endl;
    }

    return 0;
}

