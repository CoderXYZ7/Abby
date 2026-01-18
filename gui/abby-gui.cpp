#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include "AbbyCrypt.hpp"

namespace fs = std::filesystem;

#define SOCKET_PATH "/tmp/abby.sock"

class AbbyGUI {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    TTF_Font* smallFont;
    bool running;
    std::string currentFile;
    std::string status;
    bool showFilePicker;
    std::vector<std::string> files;
    int selectedFileIndex;
    int scrollOffset;
    
    // Target Encryption State
    std::string targetSerial;
    bool isEditingSerial;
    bool isTargetEncryptMode;
    std::string encryptionStatus;
    
    bool isDaemonRunning() {
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) return false;
        
        struct sockaddr_un serv_addr;
        serv_addr.sun_family = AF_UNIX;
        strncpy(serv_addr.sun_path, SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);
        
        bool running = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0;
        close(sock);
        return running;
    }
    
    void startDaemon() {
        std::cout << "Starting AbbyPlayer daemon..." << std::endl;
        system("device/AbbyPlayer/build/AbbyPlayer --daemon > /dev/null 2>&1 &");
        sleep(3);
    }
    
    std::string sendCommand(const std::string& cmd) {
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) return "ERROR";
        
        struct sockaddr_un serv_addr;
        serv_addr.sun_family = AF_UNIX;
        strncpy(serv_addr.sun_path, SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);
        
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            close(sock);
            return "ERROR: Not connected";
        }
        
        send(sock, cmd.c_str(), cmd.length(), 0);
        
        char buffer[1024] = {0};
        int valread = read(sock, buffer, 1023);
        close(sock);
        
        if (valread > 0) return std::string(buffer);
        return "ERROR";
    }
    
    void loadAudioFiles() {
        files.clear();
        std::string audioDir = "device/audio";
        
        if (fs::exists(audioDir)) {
            for (const auto& entry : fs::directory_iterator(audioDir)) {
                std::string filename = entry.path().filename().string();
                if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".pira") {
                    files.push_back(entry.path().string());
                } else if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".mp3") {
                    files.push_back(entry.path().string());
                }
            }
        }
    }
    
    std::string ensureEncrypted(const std::string& filepath) {
        if (filepath.size() >= 5 && filepath.substr(filepath.size() - 5) == ".pira") {
            return filepath;
        }
        
        std::string outputPath = filepath + ".pira";
        std::cout << "Encrypting " << filepath << "..." << std::endl;
        
        if (Abby::AbbyCrypt::encryptTrackFile(filepath, outputPath, Abby::AbbyCrypt::getHardwareSerial())) {
            std::cout << "Encryption successful!" << std::endl;
            return outputPath;
        }
        return "";
    }
    
    void drawText(const std::string& text, int x, int y, SDL_Color color, TTF_Font* customFont = nullptr) {
        TTF_Font* useFont = customFont ? customFont : font;
        if (!useFont) return;
        
        SDL_Surface* surface = TTF_RenderText_Solid(useFont, text.c_str(), color);
        if (!surface) return;
        
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (!texture) {
            SDL_FreeSurface(surface);
            return;
        }
        
        SDL_Rect destRect = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, NULL, &destRect);
        
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }
    
    void drawButton(int x, int y, int w, int h, const std::string& text, SDL_Color bgColor) {
        SDL_Rect rect = {x, y, w, h};
        SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
        SDL_RenderFillRect(renderer, &rect);
        
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &rect);
        
        SDL_Color textColor = {255, 255, 255, 255};
        drawText(text, x + 10, y + 15, textColor);
    }
    
    void drawFilePickerModal() {
        // Modal background
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_Rect overlay = {0, 0, 800, 600};
        SDL_RenderFillRect(renderer, &overlay);
        
        // Modal window
        SDL_Rect modalRect = {100, 100, 600, 400};
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_RenderFillRect(renderer, &modalRect);
        SDL_SetRenderDrawColor(renderer, 100, 100, 200, 255);
        SDL_RenderDrawRect(renderer, &modalRect);
        
        // Title
        SDL_Color titleColor = {100, 200, 255, 255};
        drawText(isTargetEncryptMode ? "Select File to Encrypt" : "Select Audio File", 120, 120, titleColor);
        
        // File list
        int yPos = 160;
        int maxVisible = 10;
        for (int i = scrollOffset; i < files.size() && i < scrollOffset + maxVisible; ++i) {
            SDL_Color fileColor = (i == selectedFileIndex) ? SDL_Color{255, 255, 100, 255} : SDL_Color{200, 200, 200, 255};
            
            std::string filename = fs::path(files[i]).filename().string();
            drawText(filename, 120, yPos, fileColor, smallFont);
            yPos += 25;
        }
        
        // Buttons
        std::string actionText = isTargetEncryptMode ? "ENCRYPT" : "PLAY";
        SDL_Color actionColor = isTargetEncryptMode ? SDL_Color{200, 100, 200, 255} : SDL_Color{50, 200, 50, 255};
        
        drawButton(120, 480, 100, 40, actionText, actionColor);
        drawButton(240, 480, 100, 40, "CANCEL", {200, 50, 50, 255});
    }
    
    bool isMouseOverButton(int mx, int my, int x, int y, int w, int h) {
        return mx >= x && mx <= x + w && my >= y && my <= y + h;
    }
    
public:
    AbbyGUI() : window(nullptr), renderer(nullptr), font(nullptr), smallFont(nullptr), 
                running(true), status("STOPPED"), showFilePicker(false), 
                selectedFileIndex(0), scrollOffset(0),
                targetSerial(""), isEditingSerial(false), 
                isTargetEncryptMode(false), encryptionStatus("") {}
    
    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL Init Error: " << SDL_GetError() << std::endl;
            return false;
        }
        
        if (TTF_Init() < 0) {
            std::cerr << "TTF Init Error: " << TTF_GetError() << std::endl;
            return false;
        }
        
        window = SDL_CreateWindow("Abby GUI Control", 
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   800, 600, SDL_WINDOW_SHOWN);
        if (!window) {
            std::cerr << "Window Error: " << SDL_GetError() << std::endl;
            return false;
        }
        
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "Renderer Error: " << SDL_GetError() << std::endl;
            return false;
        }
        
        font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
        if (!font) {
            font = TTF_OpenFont("/usr/share/fonts/TTF/DejaVuSans.ttf", 16);
        }
        
        smallFont = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14);
        if (!smallFont) {
            smallFont = TTF_OpenFont("/usr/share/fonts/TTF/DejaVuSans.ttf", 14);
        }
        
        if (!isDaemonRunning()) {
            std::cout << "Daemon not running. Starting..." << std::endl;
            startDaemon();
        }
        
        loadAudioFiles();
        
        return true;
    }
    
    void handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_TEXTINPUT && isEditingSerial) {
                targetSerial += event.text.text;
            } else if (event.type == SDL_KEYDOWN && isEditingSerial) {
                if (event.key.keysym.sym == SDLK_BACKSPACE && !targetSerial.empty()) {
                    targetSerial.pop_back();
                } else if (event.key.keysym.sym == SDLK_v && (SDL_GetModState() & KMOD_CTRL)) {
                     // Simple Paste
                     if (SDL_HasClipboardText()) {
                         char* text = SDL_GetClipboardText();
                         targetSerial += text;
                         SDL_free(text);
                     }
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                int mx = event.button.x;
                int my = event.button.y;
                
                if (showFilePicker) {
                    // File selection
                    int fileY = 160;
                    int maxVisible = 10;
                    for (int i = scrollOffset; i < files.size() && i < scrollOffset + maxVisible; ++i) {
                        if (my >= fileY && my < fileY + 25) {
                            selectedFileIndex = i;
                        }
                        fileY += 25;
                    }
                    
                    // Modal buttons
                    if (isMouseOverButton(mx, my, 120, 480, 100, 40)) { // PLAY / ENCRYPT
                        if (selectedFileIndex >= 0 && selectedFileIndex < files.size()) {
                            if (isTargetEncryptMode) {
                                // Encrypt for target
                                std::string inputPath = files[selectedFileIndex];
                                std::string baseName = fs::path(inputPath).stem().string(); // remove extension
                                if (baseName.find(".mp3") != std::string::npos) baseName = fs::path(baseName).stem().string();
                                
                                std::string outputPath = "device/audio/" + baseName + "_target.pira";
                                
                                encryptionStatus = "Encrypting...";
                                render(); // Force update
                                
                                if (Abby::AbbyCrypt::encryptTrackFile(inputPath, outputPath, targetSerial)) {
                                    encryptionStatus = "SUCCESS: " + outputPath;
                                } else {
                                    encryptionStatus = "ERROR: Encryption failed";
                                }
                            } else {
                                // Default Play
                                std::string piraPath = ensureEncrypted(files[selectedFileIndex]);
                                if (!piraPath.empty()) {
                                    sendCommand("play " + piraPath);
                                }
                            }
                        }
                        showFilePicker = false;
                        isTargetEncryptMode = false;
                    }
                    if (isMouseOverButton(mx, my, 240, 480, 100, 40)) { // CANCEL
                        showFilePicker = false;
                        isTargetEncryptMode = false;
                    }
                } else {
                    // Main controls
                    // Play Button
                    if (isMouseOverButton(mx, my, 50, 100, 150, 50)) {
                        showFilePicker = true;
                        isTargetEncryptMode = false;
                        loadAudioFiles();
                    }
                    if (isMouseOverButton(mx, my, 220, 100, 150, 50)) {
                        sendCommand("stop");
                    }
                    if (isMouseOverButton(mx, my, 390, 100, 150, 50)) {
                        sendCommand("shader next");
                    }
                    if (isMouseOverButton(mx, my, 560, 100, 150, 50)) {
                        sendCommand("shader prev");
                    }
                    // Toggle visuals button
                    if (isMouseOverButton(mx, my, 50, 170, 150, 50)) {
                        std::string visualsStatus = sendCommand("visuals status");
                        if (visualsStatus.find("STOPPED") != std::string::npos) {
                            sendCommand("visuals start");
                        } else {
                            sendCommand("visuals stop");
                        }
                    }
                    
                    // --- Target Encryption Section ---
                    
                    // Input Box Focus
                    if (mx >= 50 && mx <= 750 && my >= 380 && my <= 420) {
                        isEditingSerial = true;
                        SDL_StartTextInput();
                    } else {
                        isEditingSerial = false;
                        SDL_StopTextInput();
                    }
                    
                    // Encrypt Button
                    if (isMouseOverButton(mx, my, 50, 430, 250, 50)) {
                        if (targetSerial.empty()) {
                            encryptionStatus = "ERROR: Enter Serial First";
                        } else {
                            showFilePicker = true;
                            isTargetEncryptMode = true;
                            encryptionStatus = "Select file to encrypt...";
                            loadAudioFiles();
                        }
                    }
                }
            } else if (event.type == SDL_MOUSEWHEEL && showFilePicker) {
                if (event.wheel.y > 0) { // Scroll up
                    scrollOffset = std::max(0, scrollOffset - 1);
                } else if (event.wheel.y < 0) { // Scroll down
                    scrollOffset = std::min((int)files.size() - 10, scrollOffset + 1);
                }
            }
        }
    }
    
    void render() {
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        
        SDL_Color titleColor = {100, 200, 255, 255};
        drawText("=== ABBY GUI CONTROL ===", 250, 30, titleColor);
        
        SDL_Color playColor = {50, 200, 50, 255};
        SDL_Color stopColor = {200, 50, 50, 255};
        SDL_Color shaderColor = {100, 100, 200, 255};
        
        drawButton(50, 100, 150, 50, "SELECT FILE", playColor);
        drawButton(220, 100, 150, 50, "STOP", stopColor);
        drawButton(390, 100, 150, 50, "NEXT SHADER", shaderColor);
        drawButton(560, 100, 150, 50, "PREV SHADER", shaderColor);
        
        // Toggle visuals button
        SDL_Color visualsColor = {200, 100, 50, 255};
        drawButton(50, 170, 150, 50, "TOGGLE VIZ", visualsColor);
        
        // Status Box
        SDL_Rect statusRect = {50, 240, 700, 100};
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(renderer, &statusRect);
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderDrawRect(renderer, &statusRect);
        
        SDL_Color statusColor = {200, 200, 200, 255};
        drawText("Status:", 60, 210, statusColor);
        drawText(status, 60, 240, statusColor);
        
        // --- Target Encryption Section ---
        SDL_Color sectColor = {255, 200, 100, 255};
        drawText("Target Device Encryption:", 50, 360, sectColor);
        
        // Input Box
        SDL_Rect inputRect = {50, 380, 700, 40};
        SDL_SetRenderDrawColor(renderer, isEditingSerial ? 60 : 40, isEditingSerial ? 60 : 40, isEditingSerial ? 60 : 40, 255);
        SDL_RenderFillRect(renderer, &inputRect);
        SDL_SetRenderDrawColor(renderer, isEditingSerial ? 255 : 150, isEditingSerial ? 255 : 150, isEditingSerial ? 255 : 150, 255);
        SDL_RenderDrawRect(renderer, &inputRect);
        
        std::string displaySerial = targetSerial.empty() ? (isEditingSerial ? "" : "Enter Target Serial ID...") : targetSerial;
        SDL_Color inputTextColor = targetSerial.empty() && !isEditingSerial ? SDL_Color{100, 100, 100, 255} : SDL_Color{255, 255, 255, 255};
        drawText(displaySerial, 60, 390, inputTextColor);
        
        // Encrypt Button
        SDL_Color encryptBtnColor = {150, 50, 200, 255};
        drawButton(50, 430, 250, 50, "ENCRYPT FOR TARGET", encryptBtnColor);
        
        // Encryption Status
        if (!encryptionStatus.empty()) {
            SDL_Color resColor = encryptionStatus.find("ERROR") != std::string::npos ? SDL_Color{255, 100, 100, 255} : SDL_Color{100, 255, 100, 255};
            drawText(encryptionStatus, 320, 445, resColor, smallFont);
        }
        
        if (showFilePicker) {
            drawFilePickerModal();
        }
        
        SDL_RenderPresent(renderer);
    }
    
    void run() {
        while (running) {
            handleEvents();
            
            static int frameCount = 0;
            if (frameCount++ % 30 == 0) {
                status = sendCommand("status");
            }
            
            render();
            SDL_Delay(16);
        }
    }
    
    void cleanup() {
        if (smallFont) TTF_CloseFont(smallFont);
        if (font) TTF_CloseFont(font);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
    }
    
    ~AbbyGUI() {
        cleanup();
    }
};

int main() {
    AbbyGUI gui;
    
    if (!gui.init()) {
        std::cerr << "Failed to initialize GUI" << std::endl;
        return 1;
    }
    
    std::cout << "Abby GUI Control started" << std::endl;
    gui.run();
    
    return 0;
}
