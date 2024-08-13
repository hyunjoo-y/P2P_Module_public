#include "NetworkInterfacesFactory.h"
#include <iostream>

int main() {
    // NetworkInterfacesFactory를 통해 NetworkInterfaces 인스턴스 생성
    std::unique_ptr<NetworkInterfaces> networkInterfaces = createNetworkInterfaces();

    // 로컬 IP 주소 가져오기
    std::vector<std::string> addresses = networkInterfaces->getLocalIPAddresses();

    // 가져온 주소들 출력
    std::cout << "Local IP addresses:" << std::endl;
    for (const auto& address : addresses) {
        std::cout << address << std::endl;
    }

    return 0;
}

