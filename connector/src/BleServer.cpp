#include "BleServer.hpp"
#include <iostream>
#include <cstring>
#include <systemd/sd-bus.h>

// BlueZ D-Bus interfaces
#define BLUEZ_SERVICE "org.bluez"
#define GATT_MANAGER_IFACE "org.bluez.GattManager1"
#define GATT_SERVICE_IFACE "org.bluez.GattService1"
#define GATT_CHAR_IFACE "org.bluez.GattCharacteristic1"
#define LE_ADVERTISING_MANAGER_IFACE "org.bluez.LEAdvertisingManager1"
#define LE_ADVERTISEMENT_IFACE "org.bluez.LEAdvertisement1"
#define OBJECT_MANAGER_IFACE "org.freedesktop.DBus.ObjectManager"
#define PROPERTIES_IFACE "org.freedesktop.DBus.Properties"

BleServer::BleServer() = default;

BleServer::~BleServer() {
    stop();
}

void BleServer::setCommandHandler(CommandHandler handler) {
    m_commandHandler = std::move(handler);
}

bool BleServer::start(const std::string& deviceName) {
    if (m_running) {
        std::cerr << "[BLE] Already running" << std::endl;
        return false;
    }
    
    m_deviceName = deviceName;
    std::cout << "[BLE] Starting GATT server as '" << deviceName << "'" << std::endl;
    std::cout << "[BLE] Service UUID: " << SERVICE_UUID << std::endl;
    
    if (!setupDbus()) {
        std::cerr << "[BLE] Failed to setup D-Bus connection" << std::endl;
        return false;
    }
    
    m_running = true;
    m_serverThread = std::thread(&BleServer::serverLoop, this);
    
    std::cout << "[BLE] GATT server started successfully" << std::endl;
    return true;
}

void BleServer::stop() {
    if (!m_running) return;
    
    m_running = false;
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    
    cleanupDbus();
    std::cout << "[BLE] Server stopped" << std::endl;
}

void BleServer::notifyResponse(const std::string& response) {
    if (!m_running) return;
    m_lastResponse = response;
    sendPropertyChanged(CHAR_RSP_PATH, response);
}

void BleServer::notifyStatus(const std::string& status) {
    if (!m_running) return;
    m_currentStatus = status;
    sendPropertyChanged(CHAR_STS_PATH, status);
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

bool BleServer::setupDbus() {
    int r;
    
    // Connect to system bus
    r = sd_bus_open_system(&m_bus);
    if (r < 0) {
        std::cerr << "[BLE] Failed to connect to system bus: " << strerror(-r) << std::endl;
        return false;
    }
    std::cout << "[BLE] Connected to system D-Bus" << std::endl;
    
    // Register the GATT application with BlueZ
    if (!registerGattApplication()) {
        std::cerr << "[BLE] Failed to register GATT application" << std::endl;
        return false;
    }
    
    // Register advertisement
    if (!registerAdvertisement()) {
        std::cerr << "[BLE] Failed to register advertisement" << std::endl;
        // Continue anyway - advertising might be disabled
    }
    
    return true;
}

// D-Bus vtable for ObjectManager interface
static const sd_bus_vtable object_manager_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetManagedObjects", "", "a{oa{sa{sv}}}", BleServer::handleGetManagedObjects, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

// D-Bus vtable for GattCharacteristic1 interface
static const sd_bus_vtable gatt_char_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("ReadValue", "a{sv}", "ay", BleServer::handleCharRead, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("WriteValue", "aya{sv}", "", BleServer::handleCharWrite, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("StartNotify", "", "", BleServer::handleCharStartNotify, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("StopNotify", "", "", BleServer::handleCharStopNotify, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

bool BleServer::registerGattApplication() {
    int r;
    
    // Register ObjectManager at our root path
    r = sd_bus_add_object_vtable(m_bus, &m_slot, GATT_PATH,
                                  OBJECT_MANAGER_IFACE, object_manager_vtable, this);
    if (r < 0) {
        std::cerr << "[BLE] Failed to add ObjectManager vtable: " << strerror(-r) << std::endl;
        return false;
    }
    
    // Register characteristics vtables
    r = sd_bus_add_object_vtable(m_bus, nullptr, CHAR_CMD_PATH,
                                  GATT_CHAR_IFACE, gatt_char_vtable, this);
    r = sd_bus_add_object_vtable(m_bus, nullptr, CHAR_RSP_PATH,
                                  GATT_CHAR_IFACE, gatt_char_vtable, this);
    r = sd_bus_add_object_vtable(m_bus, nullptr, CHAR_STS_PATH,
                                  GATT_CHAR_IFACE, gatt_char_vtable, this);
    
    // Call BlueZ to register our GATT application
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    
    r = sd_bus_call_method(m_bus,
                           BLUEZ_SERVICE,
                           "/org/bluez/hci0",  // Bluetooth adapter
                           GATT_MANAGER_IFACE,
                           "RegisterApplication",
                           &error,
                           &reply,
                           "oa{sv}",
                           GATT_PATH,
                           0);  // Empty options dict
    
    if (r < 0) {
        std::cerr << "[BLE] Failed to register GATT application: " << error.message << std::endl;
        sd_bus_error_free(&error);
        // Continue anyway for testing without BlueZ
        std::cout << "[BLE] Continuing in fallback mode (no BlueZ)" << std::endl;
        return true;
    }
    
    sd_bus_message_unref(reply);
    std::cout << "[BLE] GATT application registered with BlueZ" << std::endl;
    return true;
}

bool BleServer::registerAdvertisement() {
    int r;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    
    // Register advertisement with BlueZ
    r = sd_bus_call_method(m_bus,
                           BLUEZ_SERVICE,
                           "/org/bluez/hci0",
                           LE_ADVERTISING_MANAGER_IFACE,
                           "RegisterAdvertisement",
                           &error,
                           &reply,
                           "oa{sv}",
                           ADV_PATH,
                           0);
    
    if (r < 0) {
        std::cerr << "[BLE] Failed to register advertisement: " << error.message << std::endl;
        sd_bus_error_free(&error);
        return false;
    }
    
    sd_bus_message_unref(reply);
    std::cout << "[BLE] Advertisement registered" << std::endl;
    return true;
}

void BleServer::cleanupDbus() {
    if (m_bus) {
        // Unregister from BlueZ
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_call_method(m_bus,
                           BLUEZ_SERVICE,
                           "/org/bluez/hci0",
                           GATT_MANAGER_IFACE,
                           "UnregisterApplication",
                           &error,
                           nullptr,
                           "o",
                           GATT_PATH);
        sd_bus_error_free(&error);
        
        sd_bus_slot_unref(m_slot);
        sd_bus_unref(m_bus);
        m_bus = nullptr;
        m_slot = nullptr;
    }
    std::cout << "[BLE] D-Bus cleaned up" << std::endl;
}

void BleServer::serverLoop() {
    std::cout << "[BLE] Server loop started" << std::endl;
    
    while (m_running) {
        // Process D-Bus events
        int r = sd_bus_process(m_bus, nullptr);
        if (r < 0) {
            std::cerr << "[BLE] D-Bus process error: " << strerror(-r) << std::endl;
            break;
        }
        
        if (r > 0) continue;  // More to process
        
        // Wait for next event (with timeout)
        r = sd_bus_wait(m_bus, 100000);  // 100ms timeout
        if (r < 0 && r != -EINTR) {
            std::cerr << "[BLE] D-Bus wait error: " << strerror(-r) << std::endl;
            break;
        }
    }
    
    std::cout << "[BLE] Server loop exited" << std::endl;
}

// Handle GetManagedObjects - returns our GATT service structure
int BleServer::handleGetManagedObjects(sd_bus_message* m, void* userdata, sd_bus_error* error) {
    BleServer* self = static_cast<BleServer*>(userdata);
    sd_bus_message* reply = nullptr;
    
    int r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0) return r;
    
    // Start the dictionary: a{oa{sa{sv}}}
    r = sd_bus_message_open_container(reply, 'a', "{oa{sa{sv}}}");
    if (r < 0) return r;
    
    // === Service object ===
    r = sd_bus_message_open_container(reply, 'e', "oa{sa{sv}}");
    r = sd_bus_message_append(reply, "o", SERVICE_PATH);
    r = sd_bus_message_open_container(reply, 'a', "{sa{sv}}");
    
    // GattService1 interface
    r = sd_bus_message_open_container(reply, 'e', "sa{sv}");
    r = sd_bus_message_append(reply, "s", GATT_SERVICE_IFACE);
    r = sd_bus_message_open_container(reply, 'a', "{sv}");
    
    // UUID property
    r = sd_bus_message_open_container(reply, 'e', "sv");
    r = sd_bus_message_append(reply, "s", "UUID");
    r = sd_bus_message_append(reply, "v", "s", SERVICE_UUID);
    r = sd_bus_message_close_container(reply);
    
    // Primary property
    r = sd_bus_message_open_container(reply, 'e', "sv");
    r = sd_bus_message_append(reply, "s", "Primary");
    r = sd_bus_message_append(reply, "v", "b", 1);
    r = sd_bus_message_close_container(reply);
    
    r = sd_bus_message_close_container(reply); // a{sv}
    r = sd_bus_message_close_container(reply); // sa{sv}
    r = sd_bus_message_close_container(reply); // a{sa{sv}}
    r = sd_bus_message_close_container(reply); // oa{sa{sv}}
    
    // === Command characteristic ===
    r = sd_bus_message_open_container(reply, 'e', "oa{sa{sv}}");
    r = sd_bus_message_append(reply, "o", CHAR_CMD_PATH);
    r = sd_bus_message_open_container(reply, 'a', "{sa{sv}}");
    r = sd_bus_message_open_container(reply, 'e', "sa{sv}");
    r = sd_bus_message_append(reply, "s", GATT_CHAR_IFACE);
    r = sd_bus_message_open_container(reply, 'a', "{sv}");
    
    r = sd_bus_message_open_container(reply, 'e', "sv");
    r = sd_bus_message_append(reply, "s", "UUID");
    r = sd_bus_message_append(reply, "v", "s", CHAR_COMMAND_UUID);
    r = sd_bus_message_close_container(reply);
    
    r = sd_bus_message_open_container(reply, 'e', "sv");
    r = sd_bus_message_append(reply, "s", "Service");
    r = sd_bus_message_append(reply, "v", "o", SERVICE_PATH);
    r = sd_bus_message_close_container(reply);
    
    r = sd_bus_message_open_container(reply, 'e', "sv");
    r = sd_bus_message_append(reply, "s", "Flags");
    r = sd_bus_message_open_container(reply, 'v', "as");
    r = sd_bus_message_open_container(reply, 'a', "s");
    r = sd_bus_message_append(reply, "s", "write");
    r = sd_bus_message_append(reply, "s", "write-without-response");
    r = sd_bus_message_close_container(reply);
    r = sd_bus_message_close_container(reply);
    r = sd_bus_message_close_container(reply);
    
    r = sd_bus_message_close_container(reply); // a{sv}
    r = sd_bus_message_close_container(reply); // sa{sv}
    r = sd_bus_message_close_container(reply); // a{sa{sv}}
    r = sd_bus_message_close_container(reply); // oa{sa{sv}}
    
    // === Response characteristic ===
    r = sd_bus_message_open_container(reply, 'e', "oa{sa{sv}}");
    r = sd_bus_message_append(reply, "o", CHAR_RSP_PATH);
    r = sd_bus_message_open_container(reply, 'a', "{sa{sv}}");
    r = sd_bus_message_open_container(reply, 'e', "sa{sv}");
    r = sd_bus_message_append(reply, "s", GATT_CHAR_IFACE);
    r = sd_bus_message_open_container(reply, 'a', "{sv}");
    
    r = sd_bus_message_open_container(reply, 'e', "sv");
    r = sd_bus_message_append(reply, "s", "UUID");
    r = sd_bus_message_append(reply, "v", "s", CHAR_RESPONSE_UUID);
    r = sd_bus_message_close_container(reply);
    
    r = sd_bus_message_open_container(reply, 'e', "sv");
    r = sd_bus_message_append(reply, "s", "Service");
    r = sd_bus_message_append(reply, "v", "o", SERVICE_PATH);
    r = sd_bus_message_close_container(reply);
    
    r = sd_bus_message_open_container(reply, 'e', "sv");
    r = sd_bus_message_append(reply, "s", "Flags");
    r = sd_bus_message_open_container(reply, 'v', "as");
    r = sd_bus_message_open_container(reply, 'a', "s");
    r = sd_bus_message_append(reply, "s", "notify");
    r = sd_bus_message_close_container(reply);
    r = sd_bus_message_close_container(reply);
    r = sd_bus_message_close_container(reply);
    
    r = sd_bus_message_close_container(reply);
    r = sd_bus_message_close_container(reply);
    r = sd_bus_message_close_container(reply);
    r = sd_bus_message_close_container(reply);
    
    // === Status characteristic ===
    r = sd_bus_message_open_container(reply, 'e', "oa{sa{sv}}");
    r = sd_bus_message_append(reply, "o", CHAR_STS_PATH);
    r = sd_bus_message_open_container(reply, 'a', "{sa{sv}}");
    r = sd_bus_message_open_container(reply, 'e', "sa{sv}");
    r = sd_bus_message_append(reply, "s", GATT_CHAR_IFACE);
    r = sd_bus_message_open_container(reply, 'a', "{sv}");
    
    r = sd_bus_message_open_container(reply, 'e', "sv");
    r = sd_bus_message_append(reply, "s", "UUID");
    r = sd_bus_message_append(reply, "v", "s", CHAR_STATUS_UUID);
    r = sd_bus_message_close_container(reply);
    
    r = sd_bus_message_open_container(reply, 'e', "sv");
    r = sd_bus_message_append(reply, "s", "Service");
    r = sd_bus_message_append(reply, "v", "o", SERVICE_PATH);
    r = sd_bus_message_close_container(reply);
    
    r = sd_bus_message_open_container(reply, 'e', "sv");
    r = sd_bus_message_append(reply, "s", "Flags");
    r = sd_bus_message_open_container(reply, 'v', "as");
    r = sd_bus_message_open_container(reply, 'a', "s");
    r = sd_bus_message_append(reply, "s", "read");
    r = sd_bus_message_append(reply, "s", "notify");
    r = sd_bus_message_close_container(reply);
    r = sd_bus_message_close_container(reply);
    r = sd_bus_message_close_container(reply);
    
    r = sd_bus_message_close_container(reply);
    r = sd_bus_message_close_container(reply);
    r = sd_bus_message_close_container(reply);
    r = sd_bus_message_close_container(reply);
    
    r = sd_bus_message_close_container(reply); // Close outer array
    
    return sd_bus_send(nullptr, reply, nullptr);
}

// Handle characteristic read
int BleServer::handleCharRead(sd_bus_message* m, void* userdata, sd_bus_error* error) {
    BleServer* self = static_cast<BleServer*>(userdata);
    const char* path = sd_bus_message_get_path(m);
    
    std::string value;
    if (strcmp(path, CHAR_STS_PATH) == 0) {
        value = self->m_currentStatus;
    } else if (strcmp(path, CHAR_RSP_PATH) == 0) {
        value = self->m_lastResponse;
    }
    
    sd_bus_message* reply = nullptr;
    sd_bus_message_new_method_return(m, &reply);
    
    // Return as byte array
    sd_bus_message_open_container(reply, 'a', "y");
    for (char c : value) {
        sd_bus_message_append(reply, "y", (uint8_t)c);
    }
    sd_bus_message_close_container(reply);
    
    return sd_bus_send(nullptr, reply, nullptr);
}

// Handle characteristic write (Command)
int BleServer::handleCharWrite(sd_bus_message* m, void* userdata, sd_bus_error* error) {
    BleServer* self = static_cast<BleServer*>(userdata);
    const char* path = sd_bus_message_get_path(m);
    
    if (strcmp(path, CHAR_CMD_PATH) != 0) {
        return sd_bus_error_set(error, SD_BUS_ERROR_NOT_SUPPORTED, "Write not supported");
    }
    
    // Read the byte array
    const uint8_t* data;
    size_t size;
    int r = sd_bus_message_read_array(m, 'y', (const void**)&data, &size);
    if (r < 0) {
        return sd_bus_error_set(error, SD_BUS_ERROR_INVALID_ARGS, "Invalid data");
    }
    
    std::string command(reinterpret_cast<const char*>(data), size);
    std::cout << "[BLE] Received command: " << command << std::endl;
    
    // Process command
    if (self->m_commandHandler) {
        std::string response = self->m_commandHandler(command);
        self->m_lastResponse = response;
        
        // Send response notification
        self->notifyResponse(response);
    }
    
    return sd_bus_reply_method_return(m, "");
}

int BleServer::handleCharStartNotify(sd_bus_message* m, void* userdata, sd_bus_error* error) {
    BleServer* self = static_cast<BleServer*>(userdata);
    self->m_clientConnected = true;
    std::cout << "[BLE] Client started notifications" << std::endl;
    return sd_bus_reply_method_return(m, "");
}

int BleServer::handleCharStopNotify(sd_bus_message* m, void* userdata, sd_bus_error* error) {
    BleServer* self = static_cast<BleServer*>(userdata);
    self->m_clientConnected = false;
    std::cout << "[BLE] Client stopped notifications" << std::endl;
    return sd_bus_reply_method_return(m, "");
}

void BleServer::sendPropertyChanged(const char* charPath, const std::string& value) {
    if (!m_bus) return;
    
    // Emit PropertiesChanged signal
    sd_bus_emit_signal(m_bus,
                       charPath,
                       PROPERTIES_IFACE,
                       "PropertiesChanged",
                       "sa{sv}as",
                       GATT_CHAR_IFACE,
                       1,  // One property
                       "Value",
                       "ay",
                       value.size(),
                       value.c_str(),
                       0); // No invalidated properties
}
