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

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstring>
#include <unistd.h>

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
    
public:
    AbbyWindow(int w, int h, const char* title) : Fl_Window(w, h, title) {
        this->color(fl_rgb_color(40, 40, 40));
        
        // Header
        Fl_Box* header = new Fl_Box(0, 10, w, 40, "ABBY CONTROL CENTER");
        header->labelsize(24);
        header->labelcolor(FL_WHITE);
        header->labelfont(FL_BOLD);
        
        // Controls Group
        int y = 60;
        
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
    // Ensure daemon
    std::string test = sendCommand("status");
    if (test == "ERROR") {
        std::cout << "Starting daemon..." << std::endl;
        system("device/AbbyPlayer/build/AbbyPlayer --daemon > /dev/null 2>&1 &");
        sleep(2);
    }

    AbbyWindow *window = new AbbyWindow(500, 450, "Abby Control");
    window->show(argc, argv);
    return Fl::run();
}
