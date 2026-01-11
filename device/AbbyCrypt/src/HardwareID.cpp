#include "HardwareID.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

std::string HardwareID::getSerial() {
    // Try multiple sources for hardware ID in order of preference
    
    // 1. Try /etc/machine-id (most systems)
    std::ifstream machineId("/etc/machine-id");
    if (machineId) {
        std::string id;
        std::getline(machineId, id);
        machineId.close();
        if (!id.empty()) {
            // Trim whitespace
            id.erase(id.find_last_not_of(" \n\r\t") + 1);
            return "MACHINE_" + id;
        }
    }
    
    // 2. Try /var/lib/dbus/machine-id (alternative location)
    std::ifstream dbusMachineId("/var/lib/dbus/machine-id");
    if (dbusMachineId) {
        std::string id;
        std::getline(dbusMachineId, id);
        dbusMachineId.close();
        if (!id.empty()) {
            id.erase(id.find_last_not_of(" \n\r\t") + 1);
            return "DBUS_" + id;
        }
    }
    
    // 3. Try CPU serial (Raspberry Pi specific)
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("Serial") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    std::string serial = line.substr(pos + 1);
                    // Trim whitespace
                    serial.erase(0, serial.find_first_not_of(" \t"));
                    serial.erase(serial.find_last_not_of(" \n\r\t") + 1);
                    cpuinfo.close();
                    if (!serial.empty() && serial != "0000000000000000") {
                        return "CPU_" + serial;
                    }
                }
            }
        }
        cpuinfo.close();
    }
    
    // Fallback for development
    std::cerr << "[HardwareID] Warning: Could not read hardware ID, using fallback" << std::endl;
    return "DEV_HW_ID_123456789";
}
