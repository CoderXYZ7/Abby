#pragma once
#include <string>
#include <memory>

#define ABBY_SOCKET_PATH "/tmp/abby.sock"

namespace Abby {

class AbbyClient {
public:
    AbbyClient();
    ~AbbyClient();
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    // Low-level command
    std::string sendCommand(const std::string& cmd);
    
    // Playback control
    bool play(const std::string& filepath);
    bool stop();
    bool pause();
    bool resume();
    bool seek(float seconds);
    bool setVolume(float volume); // 0.0 - 1.0
    int getVolume(); // returns 0-100
    std::string getStatus();
    
    // Visuals control
    bool startVisuals();
    bool stopVisuals();
    std::string getVisualsStatus();
    bool nextShader();
    bool prevShader();
    
    // Daemon control
    bool quit();

private:
    int m_socket;
    bool m_connected;
    
    bool ensureConnected();
};

} // namespace Abby
