#include "AbbyClient.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace Abby {

AbbyClient::AbbyClient() : m_socket(-1), m_connected(false) {}

AbbyClient::~AbbyClient() {
    disconnect();
}

bool AbbyClient::connect() {
    if (m_connected) return true;
    
    m_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_socket < 0) return false;
    
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ABBY_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (::connect(m_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    m_connected = true;
    return true;
}

void AbbyClient::disconnect() {
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }
    m_connected = false;
}

bool AbbyClient::isConnected() const {
    return m_connected;
}

bool AbbyClient::ensureConnected() {
    if (m_connected) return true;
    return connect();
}

std::string AbbyClient::sendCommand(const std::string& cmd) {
    // Use system(nc) as robust workaround for socket issues
    std::string cmdStr = "printf \"" + cmd + "\\n\" | nc -w5 -U " + std::string(ABBY_SOCKET_PATH);
    
    FILE* pipe = popen(cmdStr.c_str(), "r");
    if (!pipe) return "ERROR: popen failed";
    
    char buffer[128] = {0};
    std::string result = "";
    while (fgets(buffer, 128, pipe) != NULL) {
        result += buffer;
    }
    pclose(pipe);
    
    // Default OK if empty (fire and forget success)
    if (result.empty()) return "OK (No response)"; 
    
    // Trim
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    return result;
}

// Playback control
bool AbbyClient::play(const std::string& filepath) {
    std::string resp = sendCommand("play " + filepath);
    return resp.find("ERROR") == std::string::npos;
}

bool AbbyClient::stop() {
    std::string resp = sendCommand("stop");
    return resp.find("ERROR") == std::string::npos;
}

bool AbbyClient::pause() {
    std::string resp = sendCommand("pause");
    return resp.find("ERROR") == std::string::npos;
}

bool AbbyClient::resume() {
    std::string resp = sendCommand("resume");
    return resp.find("ERROR") == std::string::npos;
}

bool AbbyClient::seek(float seconds) {
    std::string resp = sendCommand("seek " + std::to_string(seconds));
    return resp.find("ERROR") == std::string::npos;
}

bool AbbyClient::setVolume(float volume) {
    std::string resp = sendCommand("volume " + std::to_string(volume));
    return resp.find("ERROR") == std::string::npos;
}

int AbbyClient::getVolume() {
    std::string resp = sendCommand("volume");
    // Parse "XX%" response
    try {
        return std::stoi(resp);
    } catch (...) {
        return -1;
    }
}

std::string AbbyClient::getStatus() {
    return sendCommand("status");
}

// Visuals control
bool AbbyClient::startVisuals() {
    std::string resp = sendCommand("visuals start");
    return resp.find("ERROR") == std::string::npos;
}

bool AbbyClient::stopVisuals() {
    std::string resp = sendCommand("visuals stop");
    return resp.find("ERROR") == std::string::npos;
}

std::string AbbyClient::getVisualsStatus() {
    return sendCommand("visuals status");
}

bool AbbyClient::nextShader() {
    std::string resp = sendCommand("shader next");
    return resp.find("ERROR") == std::string::npos;
}

bool AbbyClient::prevShader() {
    std::string resp = sendCommand("shader prev");
    return resp.find("ERROR") == std::string::npos;
}

// Daemon control
bool AbbyClient::quit() {
    std::string resp = sendCommand("quit");
    return resp.find("ERROR") == std::string::npos;
}

} // namespace Abby

// ========== C API Implementation ==========
#include "abby_client.h"

extern "C" {

AbbyClientHandle abby_client_create(void) {
    return new Abby::AbbyClient();
}

void abby_client_destroy(AbbyClientHandle client) {
    delete static_cast<Abby::AbbyClient*>(client);
}

int abby_client_connect(AbbyClientHandle client) {
    return static_cast<Abby::AbbyClient*>(client)->connect() ? 1 : 0;
}

void abby_client_disconnect(AbbyClientHandle client) {
    static_cast<Abby::AbbyClient*>(client)->disconnect();
}

int abby_client_is_connected(AbbyClientHandle client) {
    return static_cast<Abby::AbbyClient*>(client)->isConnected() ? 1 : 0;
}

char* abby_client_send_command(AbbyClientHandle client, const char* cmd) {
    std::string result = static_cast<Abby::AbbyClient*>(client)->sendCommand(cmd);
    char* out = (char*)malloc(result.size() + 1);
    strcpy(out, result.c_str());
    return out;
}

char* abby_client_get_status(AbbyClientHandle client) {
    std::string result = static_cast<Abby::AbbyClient*>(client)->getStatus();
    char* out = (char*)malloc(result.size() + 1);
    strcpy(out, result.c_str());
    return out;
}

int abby_client_play(AbbyClientHandle client, const char* filepath) {
    return static_cast<Abby::AbbyClient*>(client)->play(filepath) ? 1 : 0;
}

int abby_client_stop(AbbyClientHandle client) {
    return static_cast<Abby::AbbyClient*>(client)->stop() ? 1 : 0;
}

int abby_client_start_visuals(AbbyClientHandle client) {
    return static_cast<Abby::AbbyClient*>(client)->startVisuals() ? 1 : 0;
}

int abby_client_stop_visuals(AbbyClientHandle client) {
    return static_cast<Abby::AbbyClient*>(client)->stopVisuals() ? 1 : 0;
}

int abby_client_next_shader(AbbyClientHandle client) {
    return static_cast<Abby::AbbyClient*>(client)->nextShader() ? 1 : 0;
}

int abby_client_prev_shader(AbbyClientHandle client) {
    return static_cast<Abby::AbbyClient*>(client)->prevShader() ? 1 : 0;
}

} // extern "C"
