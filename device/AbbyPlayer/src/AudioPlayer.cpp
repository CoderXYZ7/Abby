#include "AbbyCrypt.hpp"
#include "AudioPlayer.hpp"
#include "FileHandler.hpp"
#include <iostream>
#include <sstream>
#include <vector>

#define MINIAUDIO_IMPLEMENTATION
#include "../include/miniaudio.h"

// Global context
struct PlayerContext {
    ma_decoder decoder;
    ma_device device;
    std::vector<unsigned char> audioData;
    bool initialized = false;
    FrequencyAnalyzer* analyzer = nullptr;
};

PlayerContext g_ctx; 

// miniaudio data callback
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    struct PlayerContext* ctx = (struct PlayerContext*)pDevice->pUserData;
    if (ctx == NULL) return;

    ma_decoder_read_pcm_frames(&ctx->decoder, pOutput, frameCount, NULL);
    
    if (ctx->analyzer) {
        ctx->analyzer->pushSamples((float*)pOutput, frameCount * ctx->decoder.outputChannels);
    }
} 

AudioPlayer::AudioPlayer() 
    : m_isPlaying(false), m_stopSignal(false), m_totalChunks(0), m_currentChunkIndex(0) {
    m_analyzer = std::make_shared<FrequencyAnalyzer>();
}

AudioPlayer::~AudioPlayer() {
    stop();
}

void AudioPlayer::play(const std::string& filepath) {
    stop();
    m_lastError = "";

    std::cout << "[AudioPlayer] Opening encrypted file: " << filepath << std::endl;
    
    m_currentFilePath = filepath;
    std::string serial = Abby::AbbyCrypt::getHardwareSerial();
    
    // Open for streaming
    if (!Abby::AbbyCrypt::openEncryptedFile(filepath, serial)) {
        std::cerr << "[AudioPlayer] Failed to open encrypted file" << std::endl;
        return;
    }
    
    m_totalChunks = Abby::AbbyCrypt::getTotalChunks();
    m_currentChunkIndex = 0;
    
    std::cout << "[AudioPlayer] Total chunks: " << m_totalChunks << std::endl;
    
    // Load ALL chunks (temporary - full streaming needs custom data source)
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_rollingBuffer.clear();
        
        std::cout << "[AudioPlayer] Decrypting all chunks..." << std::endl;
        try {
            for (size_t i = 0; i < m_totalChunks; ++i) {
                std::vector<unsigned char> chunk = Abby::AbbyCrypt::decryptNextChunk();
                if (chunk.empty()) {
                    std::cerr << "[AudioPlayer] Failed to decrypt chunk " << i << std::endl;
                    m_lastError = "Decryption Failed: Data Corruption";
                    break;
                }
                
                AudioChunk ac;
                ac.data = std::move(chunk);
                ac.chunkIndex = i;
                m_rollingBuffer.push_back(std::move(ac));
            }
        } catch (const std::exception& e) {
            std::cerr << "[AudioPlayer] Decryption Error: " << e.what() << std::endl;
            m_lastError = "Decryption Failed: Authentication Error (Wrong Key/Hardware ID)";
            m_rollingBuffer.clear();
        }
        
        // Concatenate all chunks
        g_ctx.audioData.clear();
        for (const auto& chunk : m_rollingBuffer) {
            g_ctx.audioData.insert(g_ctx.audioData.end(), chunk.data.begin(), chunk.data.end());
        }
        
        std::cout << "[AudioPlayer] Decrypted " << m_rollingBuffer.size() 
                  << " chunks, total " << g_ctx.audioData.size() << " bytes" << std::endl;
    }
    
    if (g_ctx.audioData.empty()) {
        if (m_lastError.empty()) m_lastError = "Buffer Empty (Decryption Failed?)";
        std::cerr << "[AudioPlayer] Initial buffer is empty: " << m_lastError << std::endl;
        FileHandler::closeEncryptedFile();
        return;
    }
    
    std::cout << "[AudioPlayer] Initial buffer: " << g_ctx.audioData.size() 
              << " bytes (" << m_rollingBuffer.size() << " chunks)" << std::endl;

    // Initialize decoder
    ma_decoder_config decoderConfig = ma_decoder_config_init_default();
    ma_result result = ma_decoder_init_memory(g_ctx.audioData.data(), g_ctx.audioData.size(), 
                                               &decoderConfig, &g_ctx.decoder);
    if (result != MA_SUCCESS) {
        std::cerr << "[AudioPlayer] Failed to initialize decoder" << std::endl;
        FileHandler::closeEncryptedFile();
        return;
    }
    
    g_ctx.analyzer = m_analyzer.get();

    // Initialize device
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = g_ctx.decoder.outputFormat;
    deviceConfig.playback.channels = g_ctx.decoder.outputChannels;
    deviceConfig.sampleRate        = g_ctx.decoder.outputSampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &g_ctx;

    if (ma_device_init(NULL, &deviceConfig, &g_ctx.device) != MA_SUCCESS) {
        std::cerr << "[AudioPlayer] Failed to open playback device" << std::endl;
        ma_decoder_uninit(&g_ctx.decoder);
        FileHandler::closeEncryptedFile();
        return;
    }

    if (ma_device_start(&g_ctx.device) != MA_SUCCESS) {
        std::cerr << "[AudioPlayer] Failed to start playback device" << std::endl;
        ma_device_uninit(&g_ctx.device);
        ma_decoder_uninit(&g_ctx.decoder);
        FileHandler::closeEncryptedFile();
        return;
    }

    g_ctx.initialized = true;
    m_isPlaying = true;
    m_stopSignal = false;
    
    // Launch playback monitor
    m_playbackWorker = std::thread(&AudioPlayer::playbackLoop, this, filepath);
    // Note: Background decryption disabled - all chunks loaded upfront
}

void AudioPlayer::stop() {
    if (m_isPlaying) {
        m_stopSignal = true;
        
        if (m_playbackWorker.joinable()) {
            m_playbackWorker.join();
        }
        
        m_isPlaying = false;
        std::cout << "[AudioPlayer] Stopped" << std::endl;
    }

    if (g_ctx.initialized) {
        ma_device_uninit(&g_ctx.device);
        ma_decoder_uninit(&g_ctx.decoder);
        g_ctx.initialized = false;
        g_ctx.audioData.clear();
    }
    
    FileHandler::closeEncryptedFile();
    
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_rollingBuffer.clear();
    }
}

std::string AudioPlayer::getStatus() {
    std::stringstream ss;
    if (m_isPlaying && g_ctx.initialized) {
        ma_uint64 cursor, total;
        ma_decoder_get_cursor_in_pcm_frames(&g_ctx.decoder, &cursor);
        ma_decoder_get_length_in_pcm_frames(&g_ctx.decoder, &total);
        
        float currentSec = (float)cursor / (float)g_ctx.decoder.outputSampleRate;
        float totalSec = (float)total / (float)g_ctx.decoder.outputSampleRate;
        
        ss << "PLAYING [" << (int)currentSec << "s / " << (int)totalSec << "s]";
    } else {
        if (!m_lastError.empty()) {
            ss << "ERROR: " << m_lastError;
        } else {
            ss << "STOPPED";
        }
    }
    return ss.str();
}

AudioPlayer::PlaybackState AudioPlayer::getPlaybackState() {
    PlaybackState state;
    state.isPlaying = m_isPlaying && g_ctx.initialized;
    
    if (state.isPlaying) {
        ma_uint64 cursor, total;
        ma_decoder_get_cursor_in_pcm_frames(&g_ctx.decoder, &cursor);
        ma_decoder_get_length_in_pcm_frames(&g_ctx.decoder, &total);
        
        state.currentTime = (float)cursor / (float)g_ctx.decoder.outputSampleRate;
        state.totalTime = (float)total / (float)g_ctx.decoder.outputSampleRate;
    }
    return state;
}

void AudioPlayer::playbackLoop(std::string path) {
    std::cout << "[AudioPlayer] Playback monitor started" << std::endl;
    
    while (!m_stopSignal) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        ma_uint64 cursor, total;
        ma_decoder_get_cursor_in_pcm_frames(&g_ctx.decoder, &cursor);
        ma_decoder_get_length_in_pcm_frames(&g_ctx.decoder, &total);
        
        if (cursor >= total && total > 0) {
            std::cout << "\n[AudioPlayer] Window finished, checking for more..." << std::endl;
            
            // Check if there are more chunks
            size_t nextChunk = FileHandler::getCurrentChunk();
            if (nextChunk >= m_totalChunks) {
                std::cout << "[AudioPlayer] Track complete" << std::endl;
                m_isPlaying = false;
                break;
            }
        }
    }
    
    std::cout << "[AudioPlayer] Playback monitor stopped" << std::endl;
}

void AudioPlayer::decryptionLoop(std::string path) {
    std::cout << "[AudioPlayer] Decryption thread started" << std::endl;
    
    // Initial chunks already loaded, wait a bit before streaming
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    while (!m_stopSignal) {
        size_t currentChunk = Abby::AbbyCrypt::getCurrentChunk();
        
        // Check if we need to decrypt more
        if (currentChunk < m_totalChunks) {
            std::vector<unsigned char> newChunk = Abby::AbbyCrypt::decryptNextChunk();
            
            if (!newChunk.empty()) {
                std::lock_guard<std::mutex> lock(m_bufferMutex);
                
                // Add new chunk
                AudioChunk ac;
                ac.data = std::move(newChunk);
                ac.chunkIndex = currentChunk;
                m_rollingBuffer.push_back(std::move(ac));
                
                // Remove old chunks (keep only last 5)
                while (m_rollingBuffer.size() > ROLLING_BUFFER_CHUNKS) {
                    size_t removedIdx = m_rollingBuffer.front().chunkIndex;
                    m_rollingBuffer.pop_front();
                    std::cout << "[AudioPlayer] Dropped chunk " << removedIdx 
                              << " (buffer size: " << m_rollingBuffer.size() << ")" << std::endl;
                }
                
                std::cout << "[AudioPlayer] Buffered chunk " << currentChunk 
                          << " (" << m_rollingBuffer.size() << " chunks in buffer)" << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    std::cout << "[AudioPlayer] Decryption thread stopped" << std::endl;
}
