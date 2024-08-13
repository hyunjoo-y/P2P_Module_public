cmake_minimum_required(VERSION 3.10)
project(KademliaNetwork)

set(CMAKE_CXX_STANDARD 14)
# 디버그 빌드를 위해 CMAKE_BUILD_TYPE 설정
set(CMAKE_BUILD_TYPE Debug)

# OpenSSL 라이브러리 경로 설정 (필요한 경우)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(selectRandom RelayCommunication.cpp ClientCommunication.cpp RPCClient.cpp  ../../../ICEGathering/ICECandidatesAdapter.cpp  ../../../ICEGathering/HostInterfaces/NetworkInterfacesFactory.cpp ../../../ICEGathering/HostInterfaces/NetworkInterfacesUnix.cpp ../../../ICEGathering/HostInterfaces/NetworkInterfacesWindows.cpp ../../../ICEGathering/STUNInterfaces/NATTraversalAdapter.cpp randomRelaySelection.cpp)

target_link_libraries(selectRandom ${OPENSSL_LIBRARIES} pthread curl)
