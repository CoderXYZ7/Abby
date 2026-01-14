#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <vector>
#include <mutex>
#include <deque>
#include "FrequencyAnalyzer.hpp"

#define ROLLING_BUFFER_CHUNKS 5  // 5 seconds of lookahead

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();
    
    std::shared_ptr<FrequencyAnalyzer> getAnalyzer() { return m_analyzer; }

    void play(const std::string& filepath);
    void stop();
    void pause();
    void resume();
    void seek(float seconds);
    void setVolume(float volume); // 0.0 - 1.0
    float getVolume() const;
    
    std::string getStatus();
    std::string getLastError() const { return m_lastError; }

    struct PlaybackState {
        float currentTime = 0.0f;
        float totalTime = 0.0f;
        bool isPlaying = false;
        bool isPaused = false;
        float volume = 1.0f;
    };
    
    PlaybackState getPlaybackState();
    bool isPlaying() const { return m_isPlaying && !m_isPaused; }
    bool isPaused() const { return m_isPaused; }

private:
    void playbackLoop(std::string path);
    void decryptionLoop(std::string path);

    std::atomic<bool> m_isPlaying;
    std::atomic<bool> m_isPaused;
    std::atomic<bool> m_stopSignal;
    std::atomic<float> m_volume;
    std::thread m_playbackWorker;
    std::thread m_decryptionWorker;
    
    // Rolling buffer
    struct AudioChunk {
        std::vector<unsigned char> data;
        size_t chunkIndex;
    };
    
    std::deque<AudioChunk> m_rollingBuffer;
    std::mutex m_bufferMutex;
    size_t m_totalChunks;
    std::atomic<size_t> m_currentChunkIndex;
    std::string m_currentFilePath;
    std::string m_lastError;
    
    std::shared_ptr<FrequencyAnalyzer> m_analyzer;
};
