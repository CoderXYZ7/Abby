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
#include "BleServer.hpp"
#include "PlaylistManager.hpp"

#define PORT 5000

// Globals
Abby::AbbyClient g_client;
JwtValidator g_validator("connector/keys/public.pem");
ContentCatalog g_catalog;
BleServer g_bleServer;
PlaylistManager g_playlist;

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

    // ===== AUTHENTICATION =====
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
    
    // ===== PLAYBACK COMMANDS =====
    else if (cmd == "PLAY") {
        if (!g_session.authenticated) return "ERROR: Not authenticated\n";
        
        // Check Expiry 
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
    else if (cmd == "VOLUME") {
        float vol = std::stof(args);
        g_client.setVolume(vol);
        return "OK: Volume " + std::to_string(vol) + "\n";
    }
    else if (cmd == "STATUS") {
        return g_client.getStatus() + "\n";
    }
    
    // ===== PLAYLIST COMMANDS =====
    else if (cmd == "PLAYLIST_ADD") {
        if (!g_session.authenticated) return "ERROR: Not authenticated\n";
        g_playlist.addTrack(args);
        return "OK: Added " + args + " to playlist\n";
    }
    else if (cmd == "PLAYLIST_REMOVE") {
        size_t idx = std::stoul(args);
        g_playlist.removeTrack(idx);
        return "OK: Removed track at index " + args + "\n";
    }
    else if (cmd == "PLAYLIST_CLEAR") {
        g_playlist.clearPlaylist();
        return "OK: Playlist cleared\n";
    }
    else if (cmd == "PLAYLIST_GET") {
        return g_playlist.toJson() + "\n";
    }
    else if (cmd == "PLAYLIST_NEXT") {
        std::string next = g_playlist.getNextTrack();
        if (!next.empty()) {
            return handleCommand("PLAY " + next);
        }
        return "OK: End of playlist\n";
    }
    else if (cmd == "PLAYLIST_PREV") {
        std::string prev = g_playlist.getPreviousTrack();
        if (!prev.empty()) {
            return handleCommand("PLAY " + prev);
        }
        return "OK: Start of playlist\n";
    }
    else if (cmd == "PLAYLIST_SHUFFLE") {
        bool enable = (args == "on" || args == "true" || args == "1");
        g_playlist.setShuffleEnabled(enable);
        return "OK: Shuffle " + std::string(enable ? "enabled" : "disabled") + "\n";
    }
    else if (cmd == "PLAYLIST_REPEAT") {
        if (args == "none") g_playlist.setRepeatMode(PlaylistManager::RepeatMode::NONE);
        else if (args == "one") g_playlist.setRepeatMode(PlaylistManager::RepeatMode::ONE);
        else if (args == "all") g_playlist.setRepeatMode(PlaylistManager::RepeatMode::ALL);
        return "OK: Repeat mode set to " + args + "\n";
    }
    
    // ===== CATALOG COMMANDS =====
    else if (cmd == "CATALOG_LIST") {
        return g_catalog.toJson() + "\n";
    }
    
    return "ERROR: Unknown command\n";
}

int main(int argc, char* argv[]) {
    std::cout << "[AbbyConnector] Starting..." << std::endl;
    
    // Parse command line flags
    bool bleEnabled = false;
    bool debugMode = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--ble") {
            bleEnabled = true;
        } else if (std::string(argv[i]) == "--debug") {
            debugMode = true;
        } else if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
            std::cout << "Usage: AbbyConnector [options]\n"
                      << "Options:\n"
                      << "  --debug   Interactive CLI mode (no TCP server)\n"
                      << "  --ble     Enable BLE GATT server\n"
                      << "  --help    Show this help\n";
            return 0;
        }
    }
    
    // Load catalog
    if (!g_catalog.load("connector/config/catalog.json")) {
        std::cerr << "Failed to load catalog!" << std::endl;
        return 1;
    }
    
    // Connect to AbbyPlayer daemon
    if (!g_client.connect()) {
        std::cerr << "Warning: Could not connect to AbbyPlayer daemon. Is it running?" << std::endl;
    } else {
        std::cout << "[AbbyConnector] Connected to AbbyPlayer daemon" << std::endl;
    }
    
    // Start BLE server if enabled
    if (bleEnabled) {
        std::cout << "[AbbyConnector] BLE mode enabled" << std::endl;
        g_bleServer.setCommandHandler(handleCommand);
        if (!g_bleServer.start("AbbyConnector")) {
            std::cerr << "Warning: Failed to start BLE server" << std::endl;
        }
    }
    
    // ===== DEBUG MODE: Interactive CLI =====
    if (debugMode) {
        std::cout << "\n[AbbyConnector] DEBUG MODE - Interactive CLI" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  AUTH <jwt>              - Authenticate with JWT" << std::endl;
        std::cout << "  PLAY <track_id>         - Play a track" << std::endl;
        std::cout << "  STOP / PAUSE / RESUME   - Playback control" << std::endl;
        std::cout << "  VOLUME <0.0-1.0>        - Set volume" << std::endl;
        std::cout << "  STATUS                  - Get playback status" << std::endl;
        std::cout << "  PLAYLIST_ADD <id>       - Add track to playlist" << std::endl;
        std::cout << "  PLAYLIST_GET            - Show playlist" << std::endl;
        std::cout << "  PLAYLIST_NEXT/PREV      - Navigate playlist" << std::endl;
        std::cout << "  PLAYLIST_SHUFFLE on|off - Toggle shuffle" << std::endl;
        std::cout << "  PLAYLIST_REPEAT none|one|all" << std::endl;
        std::cout << "  CATALOG_LIST            - List available tracks" << std::endl;
        std::cout << "  quit / exit             - Exit debug mode" << std::endl;
        std::cout << "========================================" << std::endl;
        
        std::string line;
        while (true) {
            std::cout << "\nabby> ";
            std::cout.flush();
            
            if (!std::getline(std::cin, line)) {
                break; // EOF
            }
            
            // Trim whitespace
            while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.erase(0, 1);
            
            if (line.empty()) continue;
            
            if (line == "quit" || line == "exit") {
                std::cout << "[AbbyConnector] Exiting debug mode..." << std::endl;
                break;
            }
            
            std::string response = handleCommand(line);
            std::cout << response;
        }
        
        return 0;
    }
    
    // ===== NORMAL MODE: TCP Server =====
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
    if (bleEnabled) {
        std::cout << "[AbbyConnector] BLE GATT server also active" << std::endl;
    }
    
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
