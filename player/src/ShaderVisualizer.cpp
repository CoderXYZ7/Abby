#include "ShaderVisualizer.hpp"
#include "ResourceManager.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

const char* ShaderVisualizer::VERTEX_SOURCE = R"(
    attribute vec2 position;
    varying vec2 v_uv;
    void main() {
        v_uv = position * 0.5 + 0.5;
        gl_Position = vec4(position, 0.0, 1.0);
    }
)";

void ShaderVisualizer::loadShaders() {
    m_programs.clear();
    
    // Use ResourceManager to find shaders directory
    std::string shaderDir = Abby::ResourceManager::instance().findDirectory("shaders");
    
    if (shaderDir.empty() || !fs::exists(shaderDir)) {
        std::cerr << "[Visuals] Shader directory not found!" << std::endl;
        return;
    }
    
    std::cout << "[Visuals] Using shader directory: " << shaderDir << std::endl;

    for (const auto& entry : fs::directory_iterator(shaderDir)) {
        if (entry.path().extension() == ".frag") {
            std::string path = entry.path().string();
            std::string name = entry.path().stem().string();
            
            std::cout << "[Visuals] Loading shader: " << name << "..." << std::endl;
            
            std::cout << "[Visuals] Loading shader file content: " << name << "..." << std::endl;
            
            std::ifstream file(path);
            if (!file) continue;
            
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string source = buffer.str();
            
            std::cout << "[Visuals] Creating program for " << name << "..." << std::endl;
            GLuint prog = createProgram(VERTEX_SOURCE, source.c_str());
            if (prog != 0) {
                m_programs.push_back({prog, name});
                std::cout << "[Visuals] Loaded " << name << " successfully." << std::endl;
            } else {
                std::cerr << "[Visuals] Failed to compile " << name << std::endl;
            }
        }
    }
    
    if (m_programs.empty()) {
        std::cerr << "[Visuals] No valid shaders found in " << shaderDir << std::endl;
    }
}

ShaderVisualizer::ShaderVisualizer(std::shared_ptr<FrequencyAnalyzer> analyzer, AudioPlayer* player) 
    : m_analyzer(analyzer), m_player(player), m_running(false), m_window(nullptr), m_glContext(nullptr) {}

ShaderVisualizer::~ShaderVisualizer() {
    requestStop();
}

bool ShaderVisualizer::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Use OpenGL ES 2.0
    // SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    // SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    // SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // Create window (Fullscreen or specific size for testing)
    // On Pi Zero without desktop, we likely want SDL_WINDOW_FULLSCREEN or SDL_WINDOW_FULLSCREEN_DESKTOP
    m_window = SDL_CreateWindow("Abby Visuals", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                                800, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN); // 800x480 common for small screens

    if (m_window == NULL) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (m_glContext == NULL) {
        std::cerr << "OpenGL context could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        std::cerr << "Error initializing GLEW: " << glewGetErrorString(glewError) << std::endl;
        return false;
    }
    std::cout << "[Visuals] OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    // Enable VSync
    SDL_GL_SetSwapInterval(1);

    // Compile Shaders
    loadShaders();
    if (m_programs.empty()) {
        std::cerr << "No shaders loaded!" << std::endl;
        return false;
    }
    // Set initial program
    m_currentProgramIndex = 0;
    // m_program is not needed as member anymore, we use m_programs[index].id

    // Fullscreen Quad Setup
    float vertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Texture Setup for Spectrum
    glGenTextures(1, &m_spectrumTexture);
    glBindTexture(GL_TEXTURE_2D, m_spectrumTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Clamp to edge to avoid artifacts at 0.0 and 1.0
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return true;
}

void ShaderVisualizer::run() {
    if (m_running) return;
    m_running = true;
    renderLoop();
}

void ShaderVisualizer::requestStop() {
    m_running = false;
    // Window destroy happens after run() returns (if called from run loop scope or destructor)
    // But for safety, we just set flag. 
    // real SDL cleanup should happen after loop exits.
}

// Destructor call to stop is now just ensuring loop exit, but we can't join itself.
// Assuming caller handles thread lifecycle.

void ShaderVisualizer::renderLoop() {
    // Make context current in this thread
    SDL_GL_MakeCurrent(m_window, m_glContext);
    
    // Prepare blank texture data
    // Prepare blank texture data
    std::vector<uint8_t> textureData(256, 0); // 256 bands, usually enough for texture
    
    float startTime = (float)SDL_GetTicks() / 1000.0f;
    float lastSwitchTime = startTime;

    while (m_running) {
        // Event Polling (Essential for SDL to not hang)
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                m_running = false;
            }
            // Allow manual switch with key
            if (e.type == SDL_KEYDOWN) {
                 if (e.key.keysym.sym == SDLK_SPACE) {
                     m_currentProgramIndex = (m_currentProgramIndex + 1) % m_programs.size();
                 }
            }
        }
        
        // 1. Get Analysis Data
        std::vector<float> spectrum = m_analyzer->getSpectrum(256);
        float pitch = m_analyzer->getDominantFrequency(44100.0f); // approx sample rate
        AudioPlayer::PlaybackState playback = m_player->getPlaybackState();
        
        // Auto-switch disabled - manual control only
        float currentTime = (float)SDL_GetTicks() / 1000.0f - startTime;
        // if (currentTime - lastSwitchTime > 10.0f) {
        //     m_currentProgramIndex = (m_currentProgramIndex + 1) % m_programs.size();
        //     lastSwitchTime = currentTime;
        // }

        // Texture Update
        for(size_t i=0; i<256 && i<spectrum.size(); ++i) {
            float val = spectrum[i] * 255.0f;
            if (val > 255.0f) val = 255.0f;
            textureData[i] = (uint8_t)val;
        }
        
        glBindTexture(GL_TEXTURE_2D, m_spectrumTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 256, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, textureData.data());

        // 2. Render
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        GLuint prog = m_programs[m_currentProgramIndex].id;
        glUseProgram(prog);
        
        GLint posAttrib = glGetAttribLocation(prog, "position");
        GLint timeUniform = glGetUniformLocation(prog, "u_time");
        GLint pitchUniform = glGetUniformLocation(prog, "u_pitch");
        GLint posUniform = glGetUniformLocation(prog, "u_pos");
        GLint durUniform = glGetUniformLocation(prog, "u_duration");
        GLint specUniform = glGetUniformLocation(prog, "u_spectrum");

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glEnableVertexAttribArray(posAttrib);
        glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
        
        glUniform1f(timeUniform, currentTime);
        glUniform1f(pitchUniform, pitch);
        glUniform1f(posUniform, playback.currentTime);
        glUniform1f(durUniform, (playback.totalTime > 0 ? playback.totalTime : 1.0f));
        glUniform1i(specUniform, 0); // Texture unit 0
        
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        
        SDL_GL_SwapWindow(m_window);
        
        SDL_Delay(16);
    }

    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

GLuint ShaderVisualizer::compileShader(GLenum type, const char* source) {
    std::cout << "[Visuals] Compiling shader type " << type << "..." << std::endl;
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        std::cerr << "[Visuals] glCreateShader failed! Error: " << glGetError() << std::endl;
        return 0;
    }
    
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        char buffer[512];
        glGetShaderInfoLog(shader, 512, NULL, buffer);
        std::cerr << "Shader Compile Error: " << buffer << std::endl;
        return 0;
    }
    std::cout << "[Visuals] Shader compiled successfully: " << shader << std::endl;
    return shader;
}

GLuint ShaderVisualizer::createProgram(const char* vertexSource, const char* fragmentSource) {
    std::cout << "[Visuals] Creating program..." << std::endl;
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    
    if (vertexShader == 0 || fragmentShader == 0) return 0;
    
    GLuint program = glCreateProgram();
    std::cout << "[Visuals] Attaching shaders to program " << program << "..." << std::endl;
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    std::cout << "[Visuals] Linking program..." << std::endl;
    glLinkProgram(program);
    
    return program;
}

void ShaderVisualizer::handleShaderCommand(const std::string& cmd) {
    if (cmd.empty()) return;
    
    if (cmd == "next") {
        m_currentProgramIndex = (m_currentProgramIndex + 1) % m_programs.size();
        std::cout << "[Visuals] Switched to shader: " << m_programs[m_currentProgramIndex].name << std::endl;
    } else if (cmd == "prev") {
        m_currentProgramIndex = (m_currentProgramIndex - 1 + m_programs.size()) % m_programs.size();
        std::cout << "[Visuals] Switched to shader: " << m_programs[m_currentProgramIndex].name << std::endl;
    } else {
        // Try to find shader by name
        for (size_t i = 0; i < m_programs.size(); ++i) {
            if (m_programs[i].name == cmd) {
                m_currentProgramIndex = i;
                std::cout << "[Visuals] Switched to shader: " << cmd << std::endl;
                return;
            }
        }
        std::cerr << "[Visuals] Shader not found: " << cmd << std::endl;
    }
}
