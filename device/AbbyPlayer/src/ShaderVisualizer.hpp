#pragma once
#include <GL/glew.h>
#include <SDL2/SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
// #if defined(__arm__) || defined(__aarch64__)
// #include <SDL2/SDL_opengles2.h>
// #else
// // Development on desktop might need standard GL or GLES simulation
// #include <SDL2/SDL_opengles2.h>
// #endif
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include "AudioPlayer.hpp" // Now needs AudioPlayer for playback state

class ShaderVisualizer {
public:
    ShaderVisualizer(std::shared_ptr<FrequencyAnalyzer> analyzer, AudioPlayer* player);
    ~ShaderVisualizer();

    bool init();
    void run(); 
    void requestStop();
    void handleShaderCommand(const std::string& cmd);

private:
    void renderLoop();
    GLuint compileShader(GLenum type, const char* source);
    GLuint createProgram(const char* vertexSource, const char* fragmentSource);

    // Shader Management
    void loadShaders();
    struct ShaderProgram {
        GLuint id;
        std::string name;
    };
    std::vector<ShaderProgram> m_programs;
    int m_currentProgramIndex = 0;

    std::shared_ptr<FrequencyAnalyzer> m_analyzer;
    AudioPlayer* m_player; // Borrowed pointer 
    
    std::atomic<bool> m_running;
    
    SDL_Window* m_window;
    SDL_GLContext m_glContext;
    
    GLuint m_vbo;
    GLuint m_spectrumTexture;
    
    static const char* VERTEX_SOURCE;
    // Fragment sources will be loaded internally
};
