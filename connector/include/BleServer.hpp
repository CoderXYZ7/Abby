#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <map>
#include <systemd/sd-bus.h>

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
    
    // D-Bus method handlers (public for vtable access)
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
    
    // D-Bus connection
    sd_bus* m_bus = nullptr;
    sd_bus_slot* m_slot = nullptr;
    
    // Object paths for BlueZ registration
    static constexpr const char* GATT_PATH = "/org/bluez/piramid";
    static constexpr const char* SERVICE_PATH = "/org/bluez/piramid/service0";
    static constexpr const char* CHAR_CMD_PATH = "/org/bluez/piramid/service0/char0";
    static constexpr const char* CHAR_RSP_PATH = "/org/bluez/piramid/service0/char1";
    static constexpr const char* CHAR_STS_PATH = "/org/bluez/piramid/service0/char2";
    static constexpr const char* ADV_PATH = "/org/bluez/piramid/advertisement0";
    
    void serverLoop();
    bool setupDbus();
    bool registerGattApplication();
    bool registerAdvertisement();
    void cleanupDbus();
    
    // Helper to send property changed signal
    void sendPropertyChanged(const char* charPath, const std::string& value);
};
