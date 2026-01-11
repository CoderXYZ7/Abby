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
    std::string getStatus();
    std::string getLastError() const { return m_lastError; }

    struct PlaybackState {
        float currentTime = 0.0f;
        float totalTime = 0.0f;
        bool isPlaying = false;
    };
    
    PlaybackState getPlaybackState();
    bool isPlaying() const { return m_isPlaying; }

private:
    void playbackLoop(std::string path);
    void decryptionLoop(std::string path);

    std::atomic<bool> m_isPlaying;
    std::atomic<bool> m_stopSignal;
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
