#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Slider.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <stdarg.h>

#include <FL/Fl_Tabs.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "AbbyCrypt.hpp"
#include "AbbyClient.hpp"

namespace fs = std::filesystem;

// Global client instance
static Abby::AbbyClient g_client;

// Wrapper for backward compatibility
std::string sendCommand(const std::string& cmd) {
    return g_client.sendCommand(cmd);
}

std::string ensureEncrypted(const std::string& filepath) {
    if (filepath.size() >= 5 && filepath.substr(filepath.size() - 5) == ".pira") {
        return filepath;
    }
    
    std::string outputPath = filepath + ".pira";
    if (Abby::AbbyCrypt::encryptTrackFile(filepath, outputPath, Abby::AbbyCrypt::getHardwareSerial())) {
        return outputPath;
    }
    return "";
}

// --- GUI Components ---

class AbbyWindow : public Fl_Window {
    Fl_Box* statusBox;
    Fl_Output* encryptionStatus;
    Fl_Input* serialInput;
    Fl_Slider* volumeSlider;
    Fl_Slider* progressSlider;
    float totalDuration = 0.0f;
    bool userSeeking = false;
    
    // Connector/Bluetooth Tab Components
    Fl_Input* elizUserInput;
    Fl_Input* elizPasswordInput;
    Fl_Input* elizTrackInput;
    Fl_Text_Buffer* tokenBuffer;
    Fl_Text_Display* tokenDisplay;
    Fl_Box* connectorStatusBox;
    
    // Bluetooth Mock Components
    Fl_Hold_Browser* bleDeviceList;
    Fl_Button* btnBleScan;
    Fl_Button* btnBleConnect;
    
    // Track List Components (after login)
    Fl_Hold_Browser* trackList;
    Fl_Button* btnPlayTrack;
    Fl_Button* btnBlePause;
    Fl_Button* btnBleStop;
    Fl_Slider* bleVolumeSlider;
    Fl_Box* blePlaybackStatus;
    std::string authToken; // Stored JWT after login
    
    // Logging Components
    Fl_Text_Buffer* logBuffer;
    Fl_Text_Display* logDisplay;
    
public:
    void log(const char* fmt, ...) {
        va_list args;
        char buffer[1024];
        va_start(args, fmt);
        vsnprintf(buffer, 1023, fmt, args);
        va_end(args);
        
        // Add timestamp?
        logBuffer->append(buffer);
        logBuffer->append("\n");
        logDisplay->scroll(logDisplay->buffer()->length(), 0.0); // Scroll to bottom
        Fl::check();
    }
    AbbyWindow(int w, int h, const char* title) : Fl_Window(w, h, title) {
        this->color(fl_rgb_color(40, 40, 40));
        
        // Header
        Fl_Box* header = new Fl_Box(0, 10, w, 40, "ABBY CONTROL CENTER");
        header->labelsize(24);
        header->labelcolor(FL_WHITE);
        header->labelfont(FL_BOLD);
        
        // --- LOG CONSOLE (Bottom) ---
        int logHeight = 150;
        int tabHeight = h - 70 - logHeight - 10;
        
        Fl_Group* grpLog = new Fl_Group(10, h - logHeight - 10, w-20, logHeight, "Log Console");
        grpLog->box(FL_FLAT_BOX);
        grpLog->color(fl_rgb_color(30, 30, 30));
        
        logBuffer = new Fl_Text_Buffer();
        logDisplay = new Fl_Text_Display(10, h - logHeight - 10 + 20, w-20, logHeight - 20, "System Logs");
        logDisplay->buffer(logBuffer);
        logDisplay->textfont(FL_COURIER);
        logDisplay->textsize(12);
        logDisplay->textcolor(FL_GREEN);
        logDisplay->color(fl_rgb_color(20, 20, 20));
        
        grpLog->end();

        // --- TABS ---
        Fl_Tabs* tabs = new Fl_Tabs(10, 60, w-20, tabHeight);
        
        // --- TAB 1: DIRECT CONTROL ---
        Fl_Group* grpDirect = new Fl_Group(10, 85, w-20, h-95, "Direct Device Control");
        grpDirect->color(fl_rgb_color(40, 40, 40));
        
        int y = 100;
        
        Fl_Button* btnSelect = new Fl_Button(20, y, 120, 40, "SELECT");
        btnSelect->color(fl_rgb_color(60, 160, 60));
        btnSelect->labelcolor(FL_WHITE);
        btnSelect->callback(cb_select, this);
        
        Fl_Button* btnPause = new Fl_Button(150, y, 80, 40, "PAUSE");
        btnPause->color(fl_rgb_color(200, 160, 40));
        btnPause->labelcolor(FL_WHITE);
        btnPause->callback(cb_pause);
        
        Fl_Button* btnStop = new Fl_Button(240, y, 80, 40, "STOP");
        btnStop->color(fl_rgb_color(160, 60, 60));
        btnStop->labelcolor(FL_WHITE);
        btnStop->callback(cb_stop);
        
        Fl_Button* btnVis = new Fl_Button(330, y, 140, 40, "TOGGLE VIZ");
        btnVis->color(fl_rgb_color(200, 120, 40));
        btnVis->labelcolor(FL_WHITE);
        btnVis->callback(cb_visuals);
        
        y += 50;
        
        // Volume slider
        Fl_Box* volLabel = new Fl_Box(20, y, 60, 30, "Vol:");
        volLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        volLabel->labelcolor(FL_WHITE);
        
        volumeSlider = new Fl_Slider(80, y, w-120, 30);
        volumeSlider->type(FL_HOR_NICE_SLIDER);
        volumeSlider->bounds(0.0, 1.0);
        volumeSlider->value(1.0);
        volumeSlider->color(fl_rgb_color(60, 60, 60));
        volumeSlider->selection_color(fl_rgb_color(100, 200, 100));
        volumeSlider->callback(cb_volume, this);
        
        y += 40;
        
        // Shader controls
        Fl_Button* btnPrev = new Fl_Button(20, y, 140, 40, "PREV SHADER");
        btnPrev->color(fl_rgb_color(80, 80, 180));
        btnPrev->labelcolor(FL_WHITE);
        btnPrev->callback(cb_prev);
        
        Fl_Button* btnNext = new Fl_Button(180, y, 140, 40, "NEXT SHADER");
        btnNext->color(fl_rgb_color(80, 80, 180));
        btnNext->labelcolor(FL_WHITE);
        btnNext->callback(cb_next);
        
        y += 50;
        
        // Status Group
        Fl_Box* statusLabel = new Fl_Box(20, y, w-40, 20, "Status:");
        statusLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        statusLabel->labelcolor(FL_WHITE);
        
        statusBox = new Fl_Box(20, y+25, w-40, 40, "Connecting...");
        statusBox->box(FL_FLAT_BOX);
        statusBox->color(fl_rgb_color(30, 30, 30));
        statusBox->labelcolor(fl_rgb_color(100, 255, 100));
        statusBox->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
        
        y += 75;
        
        // Progress slider (seek bar)
        progressSlider = new Fl_Slider(20, y, w-40, 25);
        progressSlider->type(FL_HOR_NICE_SLIDER);
        progressSlider->bounds(0.0, 1.0);
        progressSlider->value(0.0);
        progressSlider->color(fl_rgb_color(50, 50, 50));
        progressSlider->selection_color(fl_rgb_color(80, 180, 255));
        progressSlider->callback(cb_seek, this);
        
        y += 40;
        
        // Remote Encryption Group
        Fl_Box* encLabel = new Fl_Box(20, y, w-40, 20, "Target Device Encryption:");
        encLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        encLabel->labelcolor(FL_YELLOW);
        
        y += 30;
        
        serialInput = new Fl_Input(20, y, w-40, 30);
        serialInput->value("MACHINE_");
        serialInput->textcolor(FL_BLACK);
        
        y += 40;
        
        Fl_Button* btnEncrypt = new Fl_Button(20, y, 200, 40, "ENCRYPT FOR TARGET");
        btnEncrypt->color(fl_rgb_color(140, 40, 180));
        btnEncrypt->labelcolor(FL_WHITE);
        btnEncrypt->callback(cb_encrypt, this);
        
        y += 50;
        
        encryptionStatus = new Fl_Output(20, y, w-40, 30);
        encryptionStatus->box(FL_FLAT_BOX);
        encryptionStatus->color(fl_rgb_color(40, 40, 40));
        encryptionStatus->textcolor(FL_WHITE);
        encryptionStatus->clear_visible_focus();
        
        grpDirect->end();
        
        // --- TAB 2: BLUETOOTH MANAGER ---
        Fl_Group* grpConnector = new Fl_Group(10, 85, w-20, tabHeight-35, "Bluetooth Manager");
        grpConnector->color(fl_rgb_color(40, 40, 40));
        
        int cy = 100;
        
        // BLE Scanning Area
        Fl_Box* lblBle = new Fl_Box(20, cy, 100, 20, "Discovered Devices:");
        lblBle->labelcolor(FL_WHITE);
        lblBle->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        cy += 25;
        
        bleDeviceList = new Fl_Hold_Browser(20, cy, w-40, 80);
        bleDeviceList->color(fl_rgb_color(20, 20, 20));
        bleDeviceList->textcolor(FL_WHITE);
        bleDeviceList->textsize(14);
        
        cy += 90;
        
        btnBleScan = new Fl_Button(20, cy, 100, 30, "SCAN");
        btnBleScan->color(fl_rgb_color(60, 100, 160));
        btnBleScan->labelcolor(FL_WHITE);
        btnBleScan->callback(cb_ble_scan, this);
        
        btnBleConnect = new Fl_Button(130, cy, 100, 30, "CONNECT");
        btnBleConnect->color(fl_rgb_color(60, 160, 60));
        btnBleConnect->labelcolor(FL_WHITE);
        btnBleConnect->callback(cb_ble_connect, this);
        btnBleConnect->deactivate(); // Active only when device selected
        
        cy += 40;
        
        // Login / Token Area (Hidden logic, simulated via UI)
        Fl_Box* lblDev = new Fl_Box(20, cy, w-40, 1, ""); // Separator
        lblDev->box(FL_FLAT_BOX);
        lblDev->color(fl_rgb_color(100, 100, 100));
        
        cy += 10;
        
        Fl_Group* grpLogin = new Fl_Group(20, cy, w-40, 150, "App Credentials (Simulated)");
        grpLogin->box(FL_BORDER_BOX);
        grpLogin->color(fl_rgb_color(50, 50, 50));
        grpLogin->align(FL_ALIGN_TOP_LEFT | FL_ALIGN_INSIDE);
        grpLogin->labelcolor(FL_YELLOW);
        
        int ly = cy + 25;
        
        // Username
        Fl_Box* lblUser = new Fl_Box(30, ly, 70, 25, "Username:");
        lblUser->labelcolor(FL_WHITE);
        lblUser->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        elizUserInput = new Fl_Input(110, ly, 140, 25);
        elizUserInput->value("demo");
        elizUserInput->textsize(12);
        
        ly += 30;
        
        // Password
        Fl_Box* lblPass = new Fl_Box(30, ly, 70, 25, "Password:");
        lblPass->labelcolor(FL_WHITE);
        lblPass->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        elizPasswordInput = new Fl_Input(110, ly, 140, 25);
        elizPasswordInput->value("demo123");
        elizPasswordInput->textsize(12);
        elizPasswordInput->type(FL_SECRET_INPUT); // Hide password
        
        ly += 35;

        Fl_Button* btnLogin = new Fl_Button(30, ly, 120, 28, "LOGIN");
        btnLogin->color(fl_rgb_color(60, 140, 60));
        btnLogin->labelcolor(FL_WHITE);
        btnLogin->callback(cb_eliz_login, this);
        
        tokenBuffer = new Fl_Text_Buffer();
        // tokenDisplay = new Fl_Text_Display(260, cy + 20, w-300, 110);
        // tokenDisplay->buffer(tokenBuffer);
        // tokenDisplay->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
        
        grpLogin->end();
        
        cy = ly + 50; 
        
        connectorStatusBox = new Fl_Box(20, cy, w-40, 25, "Status: Not Logged In");
        connectorStatusBox->box(FL_FLAT_BOX);
        connectorStatusBox->color(fl_rgb_color(30, 30, 30));
        connectorStatusBox->labelcolor(FL_RED);
        connectorStatusBox->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
        
        cy += 35;
        
        // Track List Section (populated after login)
        Fl_Box* lblTracks = new Fl_Box(20, cy, 100, 20, "My Tracks:");
        lblTracks->labelcolor(FL_WHITE);
        lblTracks->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        cy += 25;
        
        trackList = new Fl_Hold_Browser(20, cy, w-40, 100);
        trackList->color(fl_rgb_color(20, 20, 20));
        trackList->textcolor(FL_WHITE);
        trackList->textsize(12);
        trackList->add("(Login to see your tracks)");
        
        cy += 110;
        
        btnPlayTrack = new Fl_Button(20, cy, 100, 30, "PLAY");
        btnPlayTrack->color(fl_rgb_color(60, 140, 60));
        btnPlayTrack->labelcolor(FL_WHITE);
        btnPlayTrack->callback(cb_play_track, this);
        btnPlayTrack->deactivate();
        
        btnBlePause = new Fl_Button(125, cy, 100, 30, "PAUSE");
        btnBlePause->color(fl_rgb_color(180, 140, 40));
        btnBlePause->labelcolor(FL_WHITE);
        btnBlePause->callback(cb_ble_pause, this);
        btnBlePause->deactivate();
        
        btnBleStop = new Fl_Button(230, cy, 100, 30, "STOP");
        btnBleStop->color(fl_rgb_color(180, 60, 60));
        btnBleStop->labelcolor(FL_WHITE);
        btnBleStop->callback(cb_ble_stop, this);
        btnBleStop->deactivate();
        
        cy += 40;
        
        // Volume control for BLE
        Fl_Box* lblBleVol = new Fl_Box(20, cy, 60, 25, "Volume:");
        lblBleVol->labelcolor(FL_WHITE);
        lblBleVol->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        bleVolumeSlider = new Fl_Slider(85, cy, w-110, 20);
        bleVolumeSlider->type(FL_HOR_NICE_SLIDER);
        bleVolumeSlider->bounds(0, 100);
        bleVolumeSlider->value(80);
        bleVolumeSlider->color(fl_rgb_color(50, 50, 50));
        bleVolumeSlider->selection_color(fl_rgb_color(80, 180, 255));
        bleVolumeSlider->callback(cb_ble_volume, this);
        
        cy += 30;
        
        // BLE playback status
        blePlaybackStatus = new Fl_Box(20, cy, w-40, 25, "Playback: Stopped");
        blePlaybackStatus->box(FL_FLAT_BOX);
        blePlaybackStatus->color(fl_rgb_color(30, 30, 30));
        blePlaybackStatus->labelcolor(FL_WHITE);
        blePlaybackStatus->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        grpConnector->end();
        
        tabs->end();
        end();
        
        // Start status timer
        Fl::add_timeout(1.0, cb_timer, this);
    }
    
    static void cb_select(Fl_Widget* w, void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        Fl_File_Chooser chooser("device/audio", "*.{mp3,pira}", Fl_File_Chooser::SINGLE, "Select Audio File");
        chooser.show();
        while(chooser.shown()) Fl::wait();
        
        if (chooser.value()) {
            std::string path = chooser.value();
            std::string piraPath = ensureEncrypted(path);
            if (!piraPath.empty()) {
                sendCommand("play " + piraPath);
            }
        }
    }
    
    static void cb_stop(Fl_Widget*, void*) { sendCommand("stop"); }
    static void cb_prev(Fl_Widget*, void*) { sendCommand("shader prev"); }
    static void cb_next(Fl_Widget*, void*) { sendCommand("shader next"); }
    
    static void cb_pause(Fl_Widget* w, void*) {
        std::string status = sendCommand("status");
        if (status.find("PAUSED") != std::string::npos) {
            sendCommand("resume");
            ((Fl_Button*)w)->label("PAUSE");
        } else if (status.find("PLAYING") != std::string::npos) {
            sendCommand("pause");
            ((Fl_Button*)w)->label("RESUME");
        }
    }
    
    static void cb_volume(Fl_Widget* w, void* data) {
        Fl_Slider* slider = (Fl_Slider*)w;
        float vol = slider->value();
        sendCommand("volume " + std::to_string(vol));
    }
    
    static void cb_seek(Fl_Widget* w, void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        Fl_Slider* slider = (Fl_Slider*)w;
        
        if (win->totalDuration > 0) {
            float seekTime = slider->value() * win->totalDuration;
            sendCommand("seek " + std::to_string(seekTime));
        }
    }
    
    static void cb_visuals(Fl_Widget*, void*) {
        std::string s = sendCommand("visuals status");
        if (s.find("STOPPED") != std::string::npos) sendCommand("visuals start");
        else sendCommand("visuals stop");
    }
    
    static void cb_encrypt(Fl_Widget* w, void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        const char* serial = win->serialInput->value();
        
        if (!serial || strlen(serial) == 0) {
            win->encryptionStatus->value("Error: Enter Serial ID first");
            win->encryptionStatus->textcolor(FL_RED);
            return;
        }
        
        Fl_File_Chooser chooser("device/audio", "*.{mp3,pira}", Fl_File_Chooser::SINGLE, "Select File to Encrypt");
        chooser.show();
        while(chooser.shown()) Fl::wait();
        
        if (chooser.value()) {
            std::string src = chooser.value();
            std::string baseName = fs::path(src).stem().string();
            if (baseName.find(".mp3") != std::string::npos) baseName = fs::path(baseName).stem().string();
            
            std::string dst = "device/audio/" + baseName + "_target.pira";
            
            win->encryptionStatus->value("Encrypting...");
            Fl::check(); // force redraw
            
            if (Abby::AbbyCrypt::encryptTrackFile(src, dst, serial)) {
                std::string msg = "Saved: " + dst;
                win->encryptionStatus->value(msg.c_str());
                win->encryptionStatus->textcolor(FL_GREEN);
            } else {
                win->encryptionStatus->value("Error: Encryption Failed");
                win->encryptionStatus->textcolor(FL_RED);
            }
        }
    }
    
    // --- CONNECTOR CALLBACKS ---
    
    // --- BLUETOOTH MOCK CALLBACKS ---

    static void cb_ble_scan(Fl_Widget* w, void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        win->log("[BLE] Scanning for devices...");
        win->bleDeviceList->clear();
        win->bleDeviceList->add("Scanning...");
        win->btnBleScan->deactivate();
        
        Fl::add_timeout(1.5, [](void* d) {
            AbbyWindow* W = (AbbyWindow*)d;
            W->bleDeviceList->clear();
            W->bleDeviceList->add("Abby Player (BLE) - [RSSI: -45dB]");
            W->bleDeviceList->add("Unknown Device - [RSSI: -80dB]");
            W->bleDeviceList->select(1);
            W->log("[BLE] Found 2 devices.");
            W->btnBleScan->activate();
            W->btnBleConnect->activate();
        }, win);
    }
    
    static void cb_ble_connect(Fl_Widget* w, void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        int sel = win->bleDeviceList->value();
        if (sel == 0) return;
        
        const char* txt = win->bleDeviceList->text(sel);
        win->log("[BLE] Connecting to: %s", txt);
        win->btnBleConnect->deactivate();
        win->connectorStatusBox->label("Bluetooth: Connecting...");
        
        Fl::add_timeout(1.0, [](void* d) {
             AbbyWindow* W = (AbbyWindow*)d;
             W->log("[BLE] Connected! Service Discovery complete.");
             W->log("[BLE] Service UUID: 0x180A, 0xAB84");
             W->connectorStatusBox->label("Bluetooth: Connected");
             W->connectorStatusBox->labelcolor(FL_GREEN);
             
             // Auto-authenticate via TCP to simulate BLE channel
             // If we have a token, use it.
             char* token = W->tokenBuffer->text();
             if (token && strlen(token) > 0) {
                 W->log("[Connector] Sending AUTH token via BLE...");
                 W->sendToConnector("AUTH " + std::string(token));
             } else {
                 W->log("[Connector] Connected, but no token loaded.");
             }
        }, win);
    }

    // --- CONNECTOR CALLBACKS ---
    
    static void cb_eliz_login(Fl_Widget* w, void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        const char* user = win->elizUserInput->value();
        const char* pass = win->elizPasswordInput->value();
        
        if (!user || strlen(user) == 0 || !pass || strlen(pass) == 0) {
            win->log("[LOGIN] Error: Username and password are required");
            return;
        }
        
        win->log("[LOGIN] Authenticating user: %s", user);
        
        // Build login request
        std::string cmd = "curl -s -X POST https://polserverdev.ooguy.com/index.php "
                          "-H \"Content-Type: application/json\" "
                          "-d '{\"username\": \"" + std::string(user) + "\", "
                          "\"password\": \"" + std::string(pass) + "\"}'";
                          
        win->log("[LOGIN] POST /index.php ...");
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            win->log("[LOGIN] Error: curl failed");
            return;
        }
        
        char buffer[4096];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
        
        // Check for token in response
        size_t pos = result.find("\"token\"");
        if (pos != std::string::npos) {
             size_t start = result.find("\"", pos + 8);
             size_t end = result.find("\"", start + 1);
             if (start != std::string::npos && end != std::string::npos) {
                 std::string token = result.substr(start + 1, end - start - 1);
                 win->authToken = token; // Store for later API calls
                 win->tokenBuffer->text(token.c_str());
                 win->log("[LOGIN] Success! Token received (Length: %lu)", token.length());
                 win->connectorStatusBox->label("Logged In");
                 win->connectorStatusBox->labelcolor(FL_GREEN);
                 
                 // Fetch user's tracks
                 win->fetchTracks();
                 return;
             }
        }
        
        // Check for error message
        size_t errPos = result.find("\"error\"");
        if (errPos != std::string::npos) {
            win->log("[LOGIN] Failed: %s", result.c_str());
        } else {
            win->log("[LOGIN] Failed. Response: %s", result.c_str());
        }
        win->connectorStatusBox->label("Auth Failed");
        win->connectorStatusBox->labelcolor(FL_RED);
    }
    
    void fetchTracks() {
        if (authToken.empty()) {
            log("[TRACKS] Error: Not authenticated");
            return;
        }
        
        log("[TRACKS] Fetching user's tracks...");
        
        std::string cmd = "curl -s -X GET https://polserverdev.ooguy.com/api/tracks.php "
                          "-H \"Authorization: Bearer " + authToken + "\"";
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            log("[TRACKS] Error: curl failed");
            return;
        }
        
        char buffer[4096];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
        
        // Clear track list
        trackList->clear();
        
        // Simple JSON parsing for tracks array
        // Looking for: "tracks": [{"id": "...", "name": "..."}]
        size_t tracksPos = result.find("\"tracks\"");
        if (tracksPos == std::string::npos) {
            log("[TRACKS] No tracks found or error: %s", result.c_str());
            trackList->add("(No tracks available)");
            return;
        }
        
        // Parse tracks - each object has {"id": "...", "name": "..."}
        // Find each "id" and then its corresponding "name"
        size_t pos = tracksPos;
        int count = 0;
        while ((pos = result.find("\"id\"", pos)) != std::string::npos) {
            // Get ID value
            size_t idStart = result.find("\"", pos + 5);
            size_t idEnd = result.find("\"", idStart + 1);
            if (idStart == std::string::npos || idEnd == std::string::npos) break;
            std::string trackId = result.substr(idStart + 1, idEnd - idStart - 1);
            
            // Find name after this id
            size_t namePos = result.find("\"name\"", idEnd);
            if (namePos == std::string::npos || namePos > pos + 200) {
                pos = idEnd + 1;
                continue;
            }
            
            size_t nameStart = result.find("\"", namePos + 7);
            size_t nameEnd = result.find("\"", nameStart + 1);
            if (nameStart == std::string::npos || nameEnd == std::string::npos) break;
            std::string name = result.substr(nameStart + 1, nameEnd - nameStart - 1);
            
            // Format: "TrackName [ID]"
            std::string label = name + " [" + trackId + "]";
            trackList->add(label.c_str());
            count++;
            
            pos = nameEnd + 1;
        }
        
        if (count > 0) {
            log("[TRACKS] Loaded %d tracks", count);
            btnPlayTrack->activate();
        } else {
            trackList->add("(No tracks available)");
        }
    }
    
    static void cb_play_track(Fl_Widget* w, void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        int sel = win->trackList->value();
        if (sel == 0) {
            win->log("[PLAY] No track selected");
            return;
        }
        
        const char* label = win->trackList->text(sel);
        win->log("[PLAY] Selected: %s", label);
        
        // Extract track ID from label "TrackName [TRACK_ID]"
        std::string s(label);
        size_t start = s.rfind('[');
        size_t end = s.rfind(']');
        if (start == std::string::npos || end == std::string::npos) {
            win->log("[PLAY] Error: Could not parse track ID");
            return;
        }
        std::string trackId = s.substr(start + 1, end - start - 1);
        
        win->log("[PLAY] Getting key for track: %s", trackId.c_str());
        
        // Get track key
        std::string cmd = "curl -s -X GET \"https://polserverdev.ooguy.com/api/tracks.php/" + trackId + "/key\" "
                          "-H \"Authorization: Bearer " + win->authToken + "\"";
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            win->log("[PLAY] Error: curl failed");
            return;
        }
        
        char buffer[4096];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
        
        // Check for key in response
        size_t keyPos = result.find("\"key\"");
        if (keyPos == std::string::npos) {
            win->log("[PLAY] Error: No key in response: %s", result.c_str());
            return;
        }
        
        size_t ks = result.find("\"", keyPos + 6);
        size_t ke = result.find("\"", ks + 1);
        if (ks == std::string::npos || ke == std::string::npos) {
            win->log("[PLAY] Error: Could not parse key");
            return;
        }
        std::string key = result.substr(ks + 1, ke - ks - 1);
        
        win->log("[BLE] Sending key via mock Bluetooth...");
        win->log("[BLE] Key: %s...", key.substr(0, 20).c_str());
        
        // Mock: Send play command to daemon with track file path
        // In real BLE, this would be sent over GATT characteristic
        // For now, we'll use the daemon directly
        std::string playCmd = "play device/audio/" + trackId + ".pira";
        sendCommand(playCmd);
        
        // Enable playback controls
        win->btnBlePause->activate();
        win->btnBleStop->activate();
        win->blePlaybackStatus->label("Playback: Playing");
        win->blePlaybackStatus->labelcolor(FL_GREEN);
        
        win->log("[PLAY] Track playback started via mock BLE");
    }
    
    static void cb_ble_pause(Fl_Widget* w, void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        
        std::string status = sendCommand("status");
        if (status.find("PAUSED") != std::string::npos) {
            // Currently paused, resume
            sendCommand("resume");
            win->log("[BLE] Resuming playback via mock BLE");
            win->blePlaybackStatus->label("Playback: Playing");
            win->blePlaybackStatus->labelcolor(FL_GREEN);
            win->btnBlePause->label("PAUSE");
        } else {
            // Currently playing, pause
            sendCommand("pause");
            win->log("[BLE] Pausing playback via mock BLE");
            win->blePlaybackStatus->label("Playback: Paused");
            win->blePlaybackStatus->labelcolor(FL_YELLOW);
            win->btnBlePause->label("RESUME");
        }
    }
    
    static void cb_ble_stop(Fl_Widget* w, void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        
        sendCommand("stop");
        win->log("[BLE] Stopping playback via mock BLE");
        
        win->blePlaybackStatus->label("Playback: Stopped");
        win->blePlaybackStatus->labelcolor(FL_WHITE);
        win->btnBlePause->label("PAUSE");
        win->btnBlePause->deactivate();
        win->btnBleStop->deactivate();
    }
    
    static void cb_ble_volume(Fl_Widget* w, void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        Fl_Slider* slider = (Fl_Slider*)w;
        int vol = (int)slider->value();
        
        std::string cmd = "volume " + std::to_string(vol / 100.0);
        sendCommand(cmd);
        win->log("[BLE] Volume set to %d%% via mock BLE", vol);
    }
    
    void sendToConnector(const std::string& msg) {
        int sock = 0;
        struct sockaddr_in serv_addr;
        char buffer[1024] = {0};

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            connectorStatusBox->label("Socket creation error");
            return;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(5000);

        if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
            connectorStatusBox->label("Invalid Address");
            close(sock);
            return;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            log("[Connector] Error: Connection Failed (Is Connector running?)");
            close(sock);
            return;
        }

        std::string msgWithNewline = msg + "\n";
        send(sock, msgWithNewline.c_str(), msgWithNewline.length(), 0);
        log("[Connector] Sent: %s", msg.c_str());
        
        int valread = read(sock, buffer, 1024);
        close(sock);
        
        if (valread > 0) {
            buffer[valread] = 0;
            log("[Connector] Recv: %s", buffer);
            if (std::string(buffer).find("AUTH_OK") != std::string::npos) {
                 connectorStatusBox->label("Bluetooth: Authenticated");
                 connectorStatusBox->labelcolor(FL_GREEN);
            }
        }
    }
    
    static void cb_connector_auth(Fl_Widget* w, void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        char* token = win->tokenBuffer->text();
        if (!token || strlen(token) == 0) {
            win->connectorStatusBox->label("Error: No Token");
            return;
        }
        win->sendToConnector("AUTH " + std::string(token));
    }
    
    static void cb_connector_play(Fl_Widget* w, void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        const char* track = win->elizTrackInput->value();
        win->sendToConnector("PLAY " + std::string(track));
    }
    
    static void cb_timer(void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        std::string status = sendCommand("status");
        
        static char buffer[256];
        static std::string lastStatus = "";
        
        strncpy(buffer, status.c_str(), 255);
        win->statusBox->label(buffer);
        
        // Parse time from status like "PLAYING [30s / 180s]"
        size_t bracketStart = status.find('[');
        size_t slashPos = status.find('/');
        size_t bracketEnd = status.find(']');
        
        if (bracketStart != std::string::npos && slashPos != std::string::npos && bracketEnd != std::string::npos) {
            try {
                std::string currentStr = status.substr(bracketStart + 1, slashPos - bracketStart - 2);
                std::string totalStr = status.substr(slashPos + 2, bracketEnd - slashPos - 3);
                float current = std::stof(currentStr);
                float total = std::stof(totalStr);
                
                win->totalDuration = total;
                if (total > 0) {
                    win->progressSlider->value(current / total);
                    win->progressSlider->redraw();
                }
            } catch (...) {}
        }
        
        if (status.rfind("ERROR:", 0) == 0) {
            win->statusBox->labelcolor(FL_RED);
            if (status != lastStatus) {
                 fl_alert("%s", status.c_str());
            }
        } else {
             win->statusBox->labelcolor(fl_rgb_color(100, 255, 100));
        }
        lastStatus = status;

        win->statusBox->redraw();
        
        Fl::repeat_timeout(1.0, cb_timer, data);
    }
};

int main(int argc, char **argv) {
    // 1. Ensure AbbyPlayer Daemon is running
    std::string test = sendCommand("status");
    // "OK (No response)" is returned by AbbyClient when nc fails to connect (empty stdout)
    // "ERROR" is explicit error.
    // If status doesn't look like "STOPPED", "PLAYING", or "PAUSED", assume down.
    bool daemonRunning = (test.find("STOPPED") != std::string::npos) || 
                         (test.find("PLAYING") != std::string::npos) || 
                         (test.find("PAUSED") != std::string::npos);

    if (!daemonRunning) {
            std::cout << "Daemon status check failed (" << test << "). Starting AbbyPlayer daemon..." << std::endl;
            // Use full relative path assuming run from project root, or absolute
            fs::path cwd = fs::current_path();
            // std::cout << "CWD: " << cwd.string() << std::endl;
            
            // Log to /tmp/abby_daemon.log for debugging
    
        system("device/AbbyPlayer/build/AbbyPlayer --daemon > /tmp/abby_daemon.log 2>&1 &");
        sleep(2);
    }
    
    // 2. Ensure AbbyConnector is running (check port 5000)
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5000);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
         std::cout << "Starting AbbyConnector..." << std::endl;
         system("device/AbbyConnector/build/AbbyConnector > /dev/null 2>&1 &");
         sleep(2);
    }
    close(sock);

    AbbyWindow *window = new AbbyWindow(900, 850, "Abby Control Center");
    window->show(argc, argv);
    return Fl::run();
}
