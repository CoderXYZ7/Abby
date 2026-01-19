#include "BleServer.hpp"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

// Pure Bluetooth Classic RFCOMM Server
// No BLE advertising - uses standard Bluetooth Classic discovery

BleServer::BleServer() = default;

BleServer::~BleServer() {
    stop();
}

void BleServer::setCommandHandler(CommandHandler handler) {
    m_commandHandler = std::move(handler);
}

bool BleServer::start(const std::string& deviceName) {
    if (m_running) {
        std::cerr << "[BT] Already running" << std::endl;
        return false;
    }
    
    m_deviceName = deviceName;
    std::cout << "[BT] Starting Bluetooth Classic server as '" << deviceName << "'" << std::endl;
    
    // Configure Bluetooth Classic (NO BLE advertising)
    if (!setupBluetoothClassic()) {
        std::cerr << "[BT] Failed to setup Bluetooth Classic" << std::endl;
        return false;
    }
    
    m_running = true;
    m_serverThread = std::thread(&BleServer::serverLoop, this);
    
    std::cout << "[BT] RFCOMM server started successfully" << std::endl;
    return true;
}

void BleServer::stop() {
    if (!m_running) return;
    
    m_running = false;
    
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    
    std::cout << "[BT] Server stopped" << std::endl;
}

bool BleServer::setupBluetoothClassic() {
    std::cout << "[BT] Configuring Bluetooth Classic..." << std::endl;
    
    // Power on adapter
    int ret = system("sudo btmgmt power on 2>/dev/null");
    if (ret != 0) {
        std::cerr << "[BT] Warning: btmgmt power on failed" << std::endl;
    }
    
    // Set device name
    std::string nameCmd = "sudo btmgmt name '" + m_deviceName + "' 2>/dev/null";
    system(nameCmd.c_str());
    std::cout << "[BT] Set device name to '" << m_deviceName << "'" << std::endl;

    // Set IO Capability to NoInputNoOutput for Just Works pairing (no PIN)
    system("sudo btmgmt io-cap 3 2>/dev/null");
    std::cout << "[BT] Set IO capability to NoInputNoOutput (Just Works pairing)" << std::endl;
    
    // Make device discoverable and connectable (Bluetooth Classic only, NO LE advertising)
    system("sudo hciconfig hci0 piscan 2>/dev/null");
    std::cout << "[BT] Device is now discoverable (Bluetooth Classic)" << std::endl;
    
    // Register SPP service
    ret = system("sdptool add SP 2>/dev/null");
    if (ret == 0) {
        std::cout << "[BT] Registered Serial Port service" << std::endl;
    } else {
        std::cerr << "[BT] Warning: sdptool add SP failed (SDP might not work)" << std::endl;
    }
    
    return true;
}

// Keep these for API compatibility but they do nothing now
bool BleServer::startAdvertising() {
    return setupBluetoothClassic();
}

void BleServer::stopAdvertising() {
    // No BLE advertising to stop
}

void BleServer::notifyResponse(const std::string& response) {
    if (!m_running) return;
    m_lastResponse = response;
}

void BleServer::notifyStatus(const std::string& status) {
    if (!m_running) return;
    m_currentStatus = status;
}

bool BleServer::isClientConnected() const {
    return m_clientConnected;
}

void BleServer::setCurrentStatus(const std::string& status) {
    m_currentStatus = status;
}

std::string BleServer::getCurrentStatus() const {
    return m_currentStatus;
}

void BleServer::serverLoop() {
    std::cout << "[BT] Server loop started - listening for RFCOMM connections" << std::endl;
    
    // Create Bluetooth RFCOMM socket
    int server_fd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (server_fd < 0) {
        std::cerr << "[BT] Failed to create RFCOMM socket: " << strerror(errno) << std::endl;
        std::cout << "[BT] Falling back to Unix socket at /tmp/abby_ble.sock" << std::endl;
        
        // Fallback to Unix socket
        server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "[BT] Failed to create Unix socket" << std::endl;
            return;
        }
        
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, "/tmp/abby_ble.sock", sizeof(addr.sun_path) - 1);
        
        unlink("/tmp/abby_ble.sock");
        
        if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "[BT] Bind failed: " << strerror(errno) << std::endl;
            close(server_fd);
            return;
        }
    } else {
        // RFCOMM socket setup
        struct sockaddr_rc loc_addr;
        memset(&loc_addr, 0, sizeof(loc_addr));
        loc_addr.rc_family = AF_BLUETOOTH;
        bdaddr_t any_addr = {0}; 
        loc_addr.rc_bdaddr = any_addr;
        loc_addr.rc_channel = 1;  // Channel 1
        
        if (bind(server_fd, (struct sockaddr*)&loc_addr, sizeof(loc_addr)) < 0) {
            std::cerr << "[BT] RFCOMM bind failed: " << strerror(errno) << std::endl;
            close(server_fd);
            return;
        }
        
        std::cout << "[BT] RFCOMM listening on channel 1" << std::endl;
    }
    
    listen(server_fd, 1);
    
    // Set non-blocking with poll
    struct pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;
    
    while (m_running) {
        int ret = poll(&pfd, 1, 500);  // 500ms timeout
        
        if (ret > 0 && (pfd.revents & POLLIN)) {
            int client_fd = accept(server_fd, nullptr, nullptr);
            if (client_fd >= 0) {
                std::cout << "[BT] Client connected!" << std::endl;
                m_clientConnected = true;
                
                handleClient(client_fd);
                
                close(client_fd);
                m_clientConnected = false;
                std::cout << "[BT] Client disconnected" << std::endl;
            }
        }
    }
    
    close(server_fd);
    unlink("/tmp/abby_ble.sock");
    std::cout << "[BT] Server loop exited" << std::endl;
}

void BleServer::handleClient(int client_fd) {
    char buffer[4096];
    
    while (m_running) {
        struct pollfd pfd;
        pfd.fd = client_fd;
        pfd.events = POLLIN;
        
        int ret = poll(&pfd, 1, 100);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
            if (n <= 0) break;
            
            buffer[n] = '\0';
            std::string command(buffer);
            
            // Trim newlines
            while (!command.empty() && (command.back() == '\n' || command.back() == '\r')) {
                command.pop_back();
            }
            
            std::cout << "[BT] Received: " << command << std::endl;
            
            if (m_commandHandler) {
                std::string response = m_commandHandler(command);
                m_lastResponse = response;
                
                // Send response
                write(client_fd, response.c_str(), response.length());
                std::cout << "[BT] Sent: " << response.substr(0, 50) << std::endl;
            }
        }
    }
}

// Stub implementations for D-Bus handlers (not used)
int BleServer::handleGetManagedObjects(sd_bus_message* m, void* userdata, sd_bus_error* error) {
    return 0;
}

int BleServer::handleCharRead(sd_bus_message* m, void* userdata, sd_bus_error* error) {
    return 0;
}

int BleServer::handleCharWrite(sd_bus_message* m, void* userdata, sd_bus_error* error) {
    return 0;
}

int BleServer::handleCharStartNotify(sd_bus_message* m, void* userdata, sd_bus_error* error) {
    return 0;
}

int BleServer::handleCharStopNotify(sd_bus_message* m, void* userdata, sd_bus_error* error) {
    return 0;
}

void BleServer::sendPropertyChanged(const char* charPath, const std::string& value) {
    // Not used
}
