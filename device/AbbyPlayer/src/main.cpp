#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <csignal>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <thread>
#include <memory>
#include <atomic>

#include "AbbyCrypt.hpp"
#include "AudioPlayer.hpp"
#include "ShaderVisualizer.hpp"

#define SOCKET_PATH "/tmp/abby.sock"

bool g_running = true;
std::shared_ptr<ShaderVisualizer> g_visuals = nullptr;
std::thread g_visualsThread;
std::atomic<bool> g_visualsRunning{false};

// --- CLIENT MODE (Executes single command via socket) ---
void runClientMode(const std::string& cmd, const std::string& arg = "") {
    int sock = 0;
    struct sockaddr_un serv_addr;
    char buffer[1024] = {0};

    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return;
    }

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed. Is the daemon running? (Run: AbbyPlayer --daemon)" << std::endl;
        return;
    }

    std::string message = cmd;
    if (!arg.empty()) {
        message += " " + arg;
    }

    send(sock, message.c_str(), message.length(), 0);
    int valread = read(sock, buffer, 1024);
    if(valread > 0) {
        std::cout << buffer;
    }
    close(sock);
}

// --- DAEMON MODE logic ---
void signalHandler(int signum) {
    g_running = false;
}

// Flag to signal main thread to start visuals
std::atomic<bool> g_startVisualsRequested{false};
std::atomic<bool> g_visualsActive{false};

void runSocketServer(AudioPlayer& player) {
    int server_fd;
    struct sockaddr_un address;

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return; 
    }

    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);
    unlink(SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        return;
    }

    std::cout << "Listening on " << SOCKET_PATH << "..." << std::endl;

    while (g_running) {
        fd_set readfds;
        struct timeval tv;
        
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        
        tv.tv_sec = 0; 
        tv.tv_usec = 500000; // 500ms

        int activity = select(server_fd + 1, &readfds, NULL, NULL, &tv);

        if ((activity < 0) && (errno != EINTR)) {
            // error
        }
        
        if (activity > 0 && FD_ISSET(server_fd, &readfds)) {
            int client_fd;
            if ((client_fd = accept(server_fd, NULL, NULL)) < 0) {
                continue;
            }
            
            char buffer[1024] = {0};
            int valread = read(client_fd, buffer, 1024);
            if (valread > 0) {
                std::string msg(buffer);
                while(!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) msg.pop_back();

                std::cout << "[Daemon] Received: " << msg << std::endl;
                std::string response = "OK\n";

                if (msg.rfind("play", 0) == 0) {
                    if (msg.length() > 5) {
                        player.play(msg.substr(5));
                    } else response = "ERROR: Missing file path\n";
                } else if (msg == "stop") {
                    player.stop();
                } else if (msg == "status") {
                    response = player.getStatus() + "\n";
                } else if (msg.rfind("shader", 0) == 0) {
                    if (g_visuals && g_visualsActive) {
                        std::string shaderCmd = msg.substr(7);
                        g_visuals->handleShaderCommand(shaderCmd);
                        response = "Shader command sent\n";
                    } else {
                        response = "ERROR: Visuals not running\n";
                    }
                } else if (msg == "visuals start") {
                    if (!g_visualsActive) {
                        g_startVisualsRequested = true; 
                        response = "Starting visuals...\n";
                    } else {
                        response = "Visuals already running\n";
                    }
                } else if (msg == "visuals stop") {
                    if (g_visualsActive) {
                        if (g_visuals) g_visuals->requestStop(); // Signal to stop
                        response = "Stopping visuals...\n";
                    } else {
                        response = "Visuals not running\n";
                    }
                } else if (msg == "visuals status") {
                    response = g_visualsActive ? "Visuals: RUNNING\n" : "Visuals: STOPPED\n";
                } else if (msg == "quit") {
                    g_running = false;
                    response = "SHUTTING DOWN\n";
                } else response = "UNKNOWN COMMAND\n";

                send(client_fd, response.c_str(), response.length(), 0);
            }
            close(client_fd);
        }
    }
    
    close(server_fd);
    unlink(SOCKET_PATH);
}

void runHeadlessMode(AudioPlayer& player) {
    std::cout << "\n--- Abby Daemon Mode ---" << std::endl;

    // Run socket server in BACKGROUND thread
    std::thread serverThread(runSocketServer, std::ref(player));

    // Run Visuals/Main Loop in MAIN thread
    while (g_running) {
        if (g_startVisualsRequested) {
            g_startVisualsRequested = false;
            
            std::cout << "[Daemon] Starting ShaderVisualizer on Main Thread..." << std::endl;
            g_visuals = std::make_shared<ShaderVisualizer>(player.getAnalyzer(), &player);
            
            if (g_visuals->init()) {
                g_visualsActive = true;
                g_visuals->run(); // Blocks until stopped
                g_visualsActive = false;
                std::cout << "[Daemon] ShaderVisualizer stopped." << std::endl;
            } else {
                std::cerr << "[Daemon] Failed to init visuals" << std::endl;
            }
            
            g_visuals = nullptr;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (serverThread.joinable()) serverThread.join();
    
    std::cout << "Daemon shutting down..." << std::endl;
}

void showUsage() {
    std::cout << "Usage:\n";
    std::cout << "  AbbyPlayer --daemon             Start daemon (audio only)\n";
    std::cout << "  AbbyPlayer play <file>          Play a file\n";
    std::cout << "  AbbyPlayer stop                 Stop playback\n";
    std::cout << "  AbbyPlayer status               Get status\n";
    std::cout << "  AbbyPlayer quit                 Stop the daemon\n";
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    if (argc < 2) {
        showUsage();
        return 0;
    }

    std::string arg1 = argv[1];

    if (arg1 == "--daemon") {
        AudioPlayer player;
        runHeadlessMode(player);
        player.stop();
    } 
    else if (arg1 == "play") {
        if (argc < 3) {
            std::cout << "Error: play requires a file path.\n";
            return 1;
        }
        runClientMode("play", argv[2]);
    }
    else if (arg1 == "stop") {
        runClientMode("stop");
    }
    else if (arg1 == "status") {
        runClientMode("status");
    }
    else if (arg1 == "visuals") {
        if (argc < 3) {
            std::cout << "Usage: AbbyPlayer visuals [start|stop|status]\n";
            return 1;
        }
        std::string cmd = "visuals " + std::string(argv[2]);
        runClientMode(cmd);
    }
    else if (arg1 == "quit") {
        runClientMode("quit");
    }
    else {
        showUsage();
    }

    return 0;
}
