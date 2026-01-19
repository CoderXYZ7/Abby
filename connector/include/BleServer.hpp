#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <map>
#include <systemd/sd-bus.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

/**
 * BLE GATT Server for AbbyConnector
 * Uses BlueZ for advertising and RFCOMM for data transfer
 * 
 * Service UUID: 00001000-7072-6d64-5069-72616d696400
 */

class BleServer {
public:
    using CommandHandler = std::function<std::string(const std::string&)>;
    
    // UUIDs
    static constexpr const char* SERVICE_UUID = "00001000-7072-6d64-5069-72616d696400";
    static constexpr const char* CHAR_COMMAND_UUID = "00001001-7072-6d64-5069-72616d696400";
    static constexpr const char* CHAR_RESPONSE_UUID = "00001002-7072-6d64-5069-72616d696400";
    static constexpr const char* CHAR_STATUS_UUID = "00001003-7072-6d64-5069-72616d696400";
    
    BleServer();
    ~BleServer();
    
    // Set callback for incoming commands
    void setCommandHandler(CommandHandler handler);
    
    // Start/stop the BLE server
    bool start(const std::string& deviceName = "AbbyConnector");
    void stop();
    
    // Send notification to connected client
    void notifyResponse(const std::string& response);
    void notifyStatus(const std::string& status);
    
    // Check if client is connected
    bool isClientConnected() const;
    
    // Get current status to broadcast
    void setCurrentStatus(const std::string& status);
    std::string getCurrentStatus() const;
    
    // D-Bus method handlers (stubs for compatibility)
    static int handleGetManagedObjects(sd_bus_message* m, void* userdata, sd_bus_error* error);
    static int handleCharRead(sd_bus_message* m, void* userdata, sd_bus_error* error);
    static int handleCharWrite(sd_bus_message* m, void* userdata, sd_bus_error* error);
    static int handleCharStartNotify(sd_bus_message* m, void* userdata, sd_bus_error* error);
    static int handleCharStopNotify(sd_bus_message* m, void* userdata, sd_bus_error* error);

private:
    CommandHandler m_commandHandler;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_clientConnected{false};
    std::thread m_serverThread;
    std::string m_deviceName;
    std::string m_currentStatus;
    std::string m_lastResponse;
    
    void serverLoop();
    bool setupBluetoothClassic();
    bool startAdvertising();
    void stopAdvertising();
    void handleClient(int client_fd);
    void sendPropertyChanged(const char* charPath, const std::string& value);
};
