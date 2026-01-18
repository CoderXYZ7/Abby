#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <atomic>

#include "AbbyClient.hpp"
#include "JwtValidator.hpp"
#include "ContentCatalog.hpp"

#define PORT 5000

// Globals
Abby::AbbyClient g_client;
JwtValidator g_validator("device/AbbyConnector/keys/public.pem");
ContentCatalog g_catalog;

struct Session {
    bool authenticated = false;
    nlohmann::json jwtPayload;
};

Session g_session;

std::string handleCommand(const std::string& cmdLine) {
    std::cerr << "[AbbyConnector] handleCommand: " << cmdLine << std::endl;
    if (cmdLine.empty()) return "";
    
    std::string cmd, args;
    size_t spacePos = cmdLine.find(' ');
    if (spacePos != std::string::npos) {
        cmd = cmdLine.substr(0, spacePos);
        args = cmdLine.substr(spacePos + 1);
    } else {
        cmd = cmdLine;
    }
    
    // remove newline
    while (!args.empty() && (args.back() == '\n' || args.back() == '\r')) args.pop_back();
    while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r')) cmd.pop_back();
    
    std::cerr << "[AbbyConnector] cmd='" << cmd << "' args='" << args << "'" << std::endl;

    if (cmd == "AUTH") {
        auto result = g_validator.validate(args);
        if (result.valid) {
            g_session.authenticated = true;
            g_session.jwtPayload = result.payload;
            return "OK: Authenticated. Expires: " + std::to_string((long)result.payload["exp"]) + "\n";
        } else {
            g_session.authenticated = false;
            return "ERROR: " + result.error + "\n";
        }
    }
    else if (cmd == "PLAY") {
        if (!g_session.authenticated) return "ERROR: Not authenticated\n";
        
        // Check Expiry (Double check)
        long exp = g_session.jwtPayload["exp"];
        time_t now = time(NULL);
        if (now > exp) return "ERROR: License expired\n";

        std::string code = args;
        ContentCatalog::TrackInfo info;
        if (!g_catalog.resolve(code, info)) {
            return "ERROR: Track code not found\n";
        }
        
        // Check Permissions
        if (!info.requiredPermission.empty()) {
            bool hasPerm = false;
            if (g_session.jwtPayload.contains("permissions")) {
                for (const auto& perm : g_session.jwtPayload["permissions"]) {
                    if (perm == info.requiredPermission) {
                        hasPerm = true;
                        break;
                    }
                }
            }
            if (!hasPerm) return "ERROR: Permission denied for " + info.requiredPermission + "\n";
        }
        
        // Play
        std::cout << "[AbbyConnector] Sending PLAY command to daemon: " << info.path << std::endl;
        if (g_client.play(info.path)) {
            std::cout << "[AbbyConnector] Play command success" << std::endl;
            return "OK: Playing " + code + "\n";
        } else {
            std::cerr << "[AbbyConnector] Play command failed" << std::endl;
            return "ERROR: Failed to start playback\n";
        }
    }
    else if (cmd == "STOP") {
        g_client.stop();
        return "OK\n";
    }
    else if (cmd == "PAUSE") {
        g_client.pause();
        return "OK\n";
    }
    else if (cmd == "RESUME") {
        g_client.resume();
        return "OK\n";
    }
    else if (cmd == "STATUS") {
        return g_client.getStatus() + "\n";
    }
    
    return "ERROR: Unknown command\n";
}

int main() {
    std::cout << "[AbbyConnector] Starting..." << std::endl;
    
    if (!g_catalog.load("device/AbbyConnector/config/catalog.json")) {
        std::cerr << "Failed to load catalog!" << std::endl;
        return 1;
    }
    
    if (!g_client.connect()) {
        std::cerr << "Warning: Could not connect to AbbyPlayer daemon. Is it running?" << std::endl;
    }
    
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        return 1;
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        return 1;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        return 1;
    }
    
    std::cout << "[AbbyConnector] Listening on port " << PORT << std::endl;
    
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        char buffer[4096] = {0};
        ssize_t valread = read(new_socket, buffer, sizeof(buffer) - 1);
        if (valread > 0) {
            std::string response = handleCommand(std::string(buffer, valread));
            send(new_socket, response.c_str(), response.length(), 0);
        }
        close(new_socket);
    }
    
    return 0;
}
