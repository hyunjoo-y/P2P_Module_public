cmake_minimum_required(VERSION 3.10)
project(KademliaNetwork)

set(CMAKE_CXX_STANDARD 11)

# OpenSSL 라이브러리 경로 설정 (필요한 경우)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

add_executable(bootstrap_node KademliaNode.cpp bootstrap.cpp)
target_link_libraries(bootstrap_node ${OPENSSL_LIBRARIES} pthread)

add_executable(relay_node KademliaNode.cpp relay_node.cpp)
target_link_libraries(relay_node ${OPENSSL_LIBRARIES} pthread)

