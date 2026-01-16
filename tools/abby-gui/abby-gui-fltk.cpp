#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Slider.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>

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
    
    // Connector Tab Components
    Fl_Input* elizUserInput;
    Fl_Input* elizTrackInput;
    Fl_Text_Buffer* tokenBuffer;
    Fl_Text_Display* tokenDisplay;
    Fl_Box* connectorStatusBox;
    
public:
    AbbyWindow(int w, int h, const char* title) : Fl_Window(w, h, title) {
        this->color(fl_rgb_color(40, 40, 40));
        
        // Header
        Fl_Box* header = new Fl_Box(0, 10, w, 40, "ABBY CONTROL CENTER");
        header->labelsize(24);
        header->labelcolor(FL_WHITE);
        header->labelfont(FL_BOLD);
        
        // --- TABS ---
        Fl_Tabs* tabs = new Fl_Tabs(10, 60, w-20, h-70);
        
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
        
        // --- TAB 2: CONNECTOR MOCK ---
        Fl_Group* grpConnector = new Fl_Group(10, 85, w-20, h-95, "Connector Mock Test");
        grpConnector->color(fl_rgb_color(40, 40, 40));
        
        int cy = 100;
        
        // User Input
        Fl_Box* lblUser = new Fl_Box(20, cy, 60, 30, "User ID:");
        lblUser->labelcolor(FL_WHITE);
        lblUser->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        elizUserInput = new Fl_Input(90, cy, 150, 30);
        elizUserInput->value("gui_test");
        
        cy += 40;
        
        // Password Input (Visual only for now)
        Fl_Box* lblPass = new Fl_Box(20, cy, 60, 30, "Password:");
        lblPass->labelcolor(FL_WHITE);
        lblPass->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        Fl_Input* elizPassInput;
        elizPassInput = new Fl_Input(90, cy, 150, 30);
        elizPassInput->type(FL_SECRET_INPUT);
        elizPassInput->value("password");
        
        Fl_Button* btnLogin = new Fl_Button(250, cy, 120, 30, "LOGIN (Eliz)");
        btnLogin->color(fl_rgb_color(60, 100, 160));
        btnLogin->labelcolor(FL_WHITE);
        btnLogin->callback(cb_eliz_login, this);
        
        cy += 40;
        
        // Track Input
        Fl_Box* lblTrack = new Fl_Box(20, cy, 60, 30, "Track ID:");
        lblTrack->labelcolor(FL_WHITE);
        lblTrack->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        elizTrackInput = new Fl_Input(90, cy, 150, 30);
        elizTrackInput->value("TRACK_001");
        
        cy += 40;
        
        // Token Display
        Fl_Box* lblToken = new Fl_Box(20, cy, w-40, 20, "JWT Token:");
        lblToken->labelcolor(FL_WHITE);
        lblToken->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        
        cy += 20;
        
        tokenBuffer = new Fl_Text_Buffer();
        tokenDisplay = new Fl_Text_Display(20, cy, w-40, 60);
        tokenDisplay->buffer(tokenBuffer);
        tokenDisplay->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
        
        cy += 70;
        
        // Connector Controls
        Fl_Button* btnAuth = new Fl_Button(20, cy, 120, 40, "AUTH (TCP)");
        btnAuth->color(fl_rgb_color(160, 100, 60));
        btnAuth->labelcolor(FL_WHITE);
        btnAuth->callback(cb_connector_auth, this);
        
        Fl_Button* btnPlay = new Fl_Button(150, cy, 120, 40, "PLAY (TCP)");
        btnPlay->color(fl_rgb_color(60, 160, 60));
        btnPlay->labelcolor(FL_WHITE);
        btnPlay->callback(cb_connector_play, this);
        
        cy += 50;
        
        connectorStatusBox = new Fl_Box(20, cy, w-40, 40, "Ready to connect");
        connectorStatusBox->box(FL_FLAT_BOX);
        connectorStatusBox->color(fl_rgb_color(30, 30, 30));
        connectorStatusBox->labelcolor(FL_WHITE);
        connectorStatusBox->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
        
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
    
    static void cb_eliz_login(Fl_Widget* w, void* data) {
        AbbyWindow* win = (AbbyWindow*)data;
        const char* user = win->elizUserInput->value();
        const char* track = win->elizTrackInput->value();
        
        std::string cmd = "curl -s -X POST https://polserverdev.ooguy.com/index.php "
                          "-H \"Content-Type: application/json\" "
                          "-d '{\"user_id\": \"" + std::string(user) + "\", \"permissions\": [\"" + std::string(track) + "\"], \"duration_days\": 1}'";
                          
        win->connectorStatusBox->label("Requesting Token..."); 
        win->connectorStatusBox->redraw();
        Fl::check();

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            win->tokenBuffer->text("Error: popen failed");
            return;
        }
        
        char buffer[1024];
        std::string result = "";
        while (fgets(buffer, 1024, pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
        
        // Simple JSON parse for "token": "..."
        size_t pos = result.find("\"token\"");
        if (pos != std::string::npos) {
             size_t start = result.find("\"", pos + 8);
             size_t end = result.find("\"", start + 1);
             if (start != std::string::npos && end != std::string::npos) {
                 std::string token = result.substr(start + 1, end - start - 1);
                 win->tokenBuffer->text(token.c_str());
                 win->connectorStatusBox->label("Token Received!");
                 win->connectorStatusBox->labelcolor(FL_GREEN);
                 return;
             }
        }
        
        win->tokenBuffer->text(result.c_str()); // Show full error/response if parse fails
        win->connectorStatusBox->label("Login Failed");
        win->connectorStatusBox->labelcolor(FL_RED);
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
            connectorStatusBox->label("Connection Failed (Is Connector running?)");
            close(sock);
            return;
        }

        std::string msgWithNewline = msg + "\n";
        send(sock, msgWithNewline.c_str(), msgWithNewline.length(), 0);
        
        int valread = read(sock, buffer, 1024);
        close(sock);
        
        if (valread > 0) {
            buffer[valread] = 0;
            // Hacky way to update label since we are in member function 
            // but usually called from static callback with 'this'
            // We'll update the label text but string must persist? 
            // Actually FLTK copies label if we use label(copy=0 which is default? no, copy is not default)
            // Fl_Widget::copy_label() is safer.
            connectorStatusBox->copy_label(buffer);
            connectorStatusBox->labelcolor(FL_WHITE);
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
        std::cout << "CWD: " << cwd.string() << std::endl;
        
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

    AbbyWindow *window = new AbbyWindow(500, 450, "Abby Control");
    window->show(argc, argv);
    return Fl::run();
}
