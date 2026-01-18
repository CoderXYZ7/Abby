#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>

/**
 * BLE GATT Server for AbbyConnector
 * Uses BlueZ D-Bus API to expose control interface
 * 
 * Service UUID: 00001000-7072-6d64-5069-72616d696400
 * Characteristics:
 *   - Command (Write):   00001001-7072-6d64-5069-72616d696400
 *   - Response (Notify): 00001002-7072-6d64-5069-72616d696400
 *   - Status (Read):     00001003-7072-6d64-5069-72616d696400
 */

class BleServer {
public:
    using CommandHandler = std::function<std::string(const std::string&)>;
    
    BleServer();
    ~BleServer();
    
    // Set callback for incoming commands
    void setCommandHandler(CommandHandler handler);
    
    // Start/stop the BLE server
    bool start(const std::string& deviceName = "AbbyConnector");
    void stop();
    
    // Send notification to connected client
    void notifyStatus(const std::string& status);
    
    // Check if client is connected
    bool isClientConnected() const;

private:
    CommandHandler m_commandHandler;
    std::atomic<bool> m_running{false};
    std::thread m_serverThread;
    
    // D-Bus connection handle (opaque)
    void* m_dbusConnection = nullptr;
    
    void serverLoop();
    bool setupGattService();
    void cleanupGatt();
};
