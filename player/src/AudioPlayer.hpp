#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <memory>
#include <vector>
#include "../include/miniaudio.h"
#include <mutex>
#include <condition_variable>
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

    // Miniaudio callbacks
    static ma_result ds_read(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead);
    static ma_result ds_seek(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin);

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
    size_t m_readOffsetInFrontChunk; // Offset in the first chunk of the deque
    
    std::mutex m_bufferMutex;
    std::condition_variable m_bufferCV;
    
    size_t m_totalChunks;
    std::atomic<size_t> m_currentChunkIndex; // Next chunk to be decrypted
    std::atomic<size_t> m_seekTargetChunk;   // For seeking requests
    std::atomic<size_t> m_seekOffsetInChunk; // Offset within the target chunk
    std::atomic<bool> m_seekRequested;
    
    std::string m_currentFilePath;
    std::string m_lastError;
    
    std::shared_ptr<FrequencyAnalyzer> m_analyzer;
};
