#include "NATTraversalHelper.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib> // For system calls
#include <memory>
#include <stdexcept>
#include <string>
#include <array>


// Assuming that the Coturn server details are stored in these constants
//const std::string CONFIG_FILE_PATH = "/etc/coturn/turnserver.conf"; // Default for Linux and macOS
const std::string CONFIG_FILE_PATH = "./turnserver.conf"; // Default for Linux and macOS

const std::string CONFIG_FILE_PATH_WINDOWS = "C:\\coturn\\turnserver.conf"; // Example path for Windows
STUNConfig NATTraversalHelper::stunConfig_;  // 이 줄은 클래스 정의 바깥쪽에 있어야 합니다.

// Destructor
//NATTraversalHelper::NATTraversalAdapter() {}
NATTraversalHelper::NATTraversalHelper() = default;
NATTraversalHelper::~NATTraversalHelper() = default;

std::string NATTraversalHelper::execCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd, "r"), static_cast<int(*)(FILE*)>(pclose));
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

bool NATTraversalHelper::installCoturnServer() {
    std::string osType = getOperatingSystem();
    if (osType == "Linux" || osType == "macOS") {
        // Install Coturn on Linux or macOS
        std::string installCommand = osType == "Linux" ? "sudo apt-get install coturn -y" : "brew install coturn";
        int result = system(installCommand.c_str());
        return result == 0;
    } else if (osType == "Windows") {
        std::cerr << "Manual installation required for Coturn on Windows." << std::endl;
        return false; // No direct installation script available for Windows
    } else {
        std::cerr << "Unsupported operating system for Coturn installation." << std::endl;
        return false;
    }
}

bool NATTraversalHelper::startCoturnServer() {
    std::string osType = getOperatingSystem();
    std::string command;
    if (osType == "Linux" || osType == "macOS") {
        //command = "sudo systemctl restart coturn";
	command = "turnserver -c ./turnserver.conf";
    } else {
        std::cerr << "Unsupported operating system." << std::endl;
        return false;
    }
    int result = system(command.c_str());
    return result == 0;
}

bool NATTraversalHelper::stopCoturnServer() {
    std::string osType = getOperatingSystem();
    std::string command;
    if (osType == "Linux" || osType == "macOS") {
        command = "sudo systemctl stop coturn";
    } else {
        std::cerr << "Unsupported operating system." << std::endl;
        return false;
    }
    int result = system(command.c_str());
    return result == 0;
}

bool NATTraversalHelper::checkCoturnServerStatus() {
    std::string osType = getOperatingSystem();
    std::string command;
    if (osType == "Linux" || osType == "macOS") {
        command = "sudo systemctl status coturn > /dev/null";
    } else {
        std::cerr << "Unsupported operating system." << std::endl;
        return false;
    }
    int result = system(command.c_str());
    return result == 0;
}

std::string NATTraversalHelper::getOperatingSystem() {
    #ifdef _WIN32
    return "Windows";
    #elif __APPLE__
    return "macOS";
    #elif __linux__
    return "Linux";
    #else
    return "Unknown";
    #endif
}

bool NATTraversalHelper::writeConfigFile(const std::string& osType) {
    std::string filePath = (osType == "Windows") ? CONFIG_FILE_PATH_WINDOWS : CONFIG_FILE_PATH;
    if(!(osType == "windows")){
	std::ofstream configFile(filePath);
	if (configFile.is_open()) {
	    configFile << "listening-ip=0.0.0.0" << std::endl;
	    configFile << "listening-port=" << stunConfig_.port << std::endl;
	    configFile << "min-port=" << stunConfig_.min_port << std::endl;
	    configFile << "max-port=" << stunConfig_.max_port << std::endl;
	    configFile << "external-ip=" << stunConfig_.address << std::endl;
	    configFile << "verbose" << std::endl;
	    configFile << "fingerprint" << std::endl;
	    configFile << "lt-cred-mech" << std::endl;

	    // Add more Coturn configuration settings as needed
	    configFile.close();
	    return true;
	} else {
	    std::cerr << "Unable to open the configuration file." << std::endl;
	    return false;
	}
    }
    return true;
}

bool NATTraversalHelper::setConfig(uint16_t port, uint16_t min_port, uint16_t max_port) {
    std::string address = execCommand("curl ifconfig.me");
    std::cout << "The public IP address is: " << address << std::endl;
    
    stunConfig_.address = address;
    stunConfig_.port = port;
    stunConfig_.min_port = min_port;
    stunConfig_.max_port = max_port;
    writeConfigFile("Linux");

    return true; // 함수가 bool을 반환하므로, 성공적으로 설정이 완료되면 true를 반환합니다.
}

