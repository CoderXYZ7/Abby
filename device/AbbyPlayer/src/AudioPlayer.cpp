#define MINIAUDIO_IMPLEMENTATION
#include "../include/miniaudio.h"

#include "AbbyCrypt.hpp"
#include "AudioPlayer.hpp"
#include "FileHandler.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <cstring>

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

ma_result AudioPlayer::ds_read(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead) {
    AudioPlayer* player = (AudioPlayer*)pDecoder->pUserData;
    size_t bytesRead = 0;
    uint8_t* outPtr = (uint8_t*)pBufferOut;
    
    while (bytesToRead > 0) {
        std::unique_lock<std::mutex> lock(player->m_bufferMutex);
        
        // Wait for data
        player->m_bufferCV.wait_for(lock, std::chrono::milliseconds(100), [player]() {
            return !player->m_rollingBuffer.empty() || player->m_stopSignal || player->m_seekRequested;
        });
        
        if (player->m_stopSignal || player->m_seekRequested) break;
        
        if (player->m_rollingBuffer.empty()) {
            // Buffer underrun or EOF
            break;
        }
        
        // Read from front chunk
        AudioChunk& chunk = player->m_rollingBuffer.front();
        size_t available = chunk.data.size() - player->m_readOffsetInFrontChunk;
        size_t toCopy = (bytesToRead < available) ? bytesToRead : available;
        
        if (toCopy > 0) {
            std::memcpy(outPtr, chunk.data.data() + player->m_readOffsetInFrontChunk, toCopy);
            
            outPtr += toCopy;
            bytesRead += toCopy;
            bytesToRead -= toCopy;
            player->m_readOffsetInFrontChunk += toCopy;
        }
        
        // Remove chunk if fully consumed
        if (player->m_readOffsetInFrontChunk >= chunk.data.size()) {
            player->m_readOffsetInFrontChunk = 0;
            player->m_rollingBuffer.pop_front();
            player->m_bufferCV.notify_all(); // Notify producer that space is available
        }
    }
    
    if (pBytesRead) *pBytesRead = bytesRead;
    return MA_SUCCESS; 
}

ma_result AudioPlayer::ds_seek(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin) {
    return MA_SUCCESS; // Seeking disabled for now
} 

AudioPlayer::AudioPlayer() 
    : m_isPlaying(false), m_isPaused(false), m_stopSignal(false), m_volume(1.0f),
      m_totalChunks(0), m_currentChunkIndex(0) {
    m_analyzer = std::make_shared<FrequencyAnalyzer>();
}

AudioPlayer::~AudioPlayer() {
    stop();
}

void AudioPlayer::play(const std::string& filepath) {
    stop();
    m_lastError = "";

    std::cerr << "[AudioPlayer] Opening encrypted file: " << filepath << std::endl;
    
    m_currentFilePath = filepath;
    std::string serial = Abby::AbbyCrypt::getHardwareSerial();
    
    // Open for streaming
    if (!Abby::AbbyCrypt::openEncryptedFile(filepath, serial)) {
        std::cerr << "[AudioPlayer] Failed to open encrypted file" << std::endl;
        return;
    }
    
    m_totalChunks = Abby::AbbyCrypt::getTotalChunks();
    m_currentChunkIndex = 0;
    m_readOffsetInFrontChunk = 0;
    
    std::cerr << "[AudioPlayer] Total chunks: " << m_totalChunks << std::endl;
    
    // Clear buffer
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_rollingBuffer.clear();
    }
    g_ctx.audioData.clear(); // Unused in streaming mode

    m_stopSignal = false;
    m_seekRequested = false;

    // Start decryption thread to pre-buffer
    m_decryptionWorker = std::thread(&AudioPlayer::decryptionLoop, this, filepath);

    // Wait for buffer to fill slightly (prevent immediate underrun)
    {
        std::unique_lock<std::mutex> lock(m_bufferMutex);
        m_bufferCV.wait(lock, [this] { return !m_rollingBuffer.empty() || m_stopSignal; });
    }
    
    if (m_stopSignal || m_rollingBuffer.empty()) {
       std::cerr << "[AudioPlayer] Failed to pre-buffer" << std::endl;
       return;
    }

    // Initialize decoder with CUSTOM CALLBACKS
    ma_decoder_config decoderConfig = ma_decoder_config_init_default();
    ma_result result = ma_decoder_init(ds_read, ds_seek, this, &decoderConfig, &g_ctx.decoder);

    if (result != MA_SUCCESS) {
        std::cerr << "[AudioPlayer] Failed to initialize decoder: " << result << std::endl;
        FileHandler::closeEncryptedFile();
        m_stopSignal = true;
        if(m_decryptionWorker.joinable()) m_decryptionWorker.join();
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

void AudioPlayer::pause() {
    if (m_isPlaying && !m_isPaused && g_ctx.initialized) {
        ma_device_stop(&g_ctx.device);
        m_isPaused = true;
        std::cout << "[AudioPlayer] Paused" << std::endl;
    }
}

void AudioPlayer::resume() {
    if (m_isPlaying && m_isPaused && g_ctx.initialized) {
        ma_device_start(&g_ctx.device);
        m_isPaused = false;
        std::cout << "[AudioPlayer] Resumed" << std::endl;
    }
}

void AudioPlayer::seek(float seconds) {
    if (!g_ctx.initialized) return;
    
    ma_uint64 targetFrame = (ma_uint64)(seconds * g_ctx.decoder.outputSampleRate);
    ma_decoder_seek_to_pcm_frame(&g_ctx.decoder, targetFrame);
    std::cout << "[AudioPlayer] Seeked to " << seconds << "s" << std::endl;
}

void AudioPlayer::setVolume(float volume) {
    m_volume = (volume < 0.0f) ? 0.0f : (volume > 1.0f) ? 1.0f : volume;
    if (g_ctx.initialized) {
        ma_device_set_master_volume(&g_ctx.device, m_volume);
    }
    std::cout << "[AudioPlayer] Volume set to " << (int)(m_volume * 100) << "%" << std::endl;
}

float AudioPlayer::getVolume() const {
    return m_volume;
}

std::string AudioPlayer::getStatus() {
    std::stringstream ss;
    if (m_isPlaying && g_ctx.initialized) {
        ma_uint64 cursor, total;
        ma_decoder_get_cursor_in_pcm_frames(&g_ctx.decoder, &cursor);
        ma_decoder_get_length_in_pcm_frames(&g_ctx.decoder, &total);
        
        float currentSec = (float)cursor / (float)g_ctx.decoder.outputSampleRate;
        float totalSec = (float)total / (float)g_ctx.decoder.outputSampleRate;
        
        if (m_isPaused) {
            ss << "PAUSED [" << (int)currentSec << "s / " << (int)totalSec << "s]";
        } else {
            ss << "PLAYING [" << (int)currentSec << "s / " << (int)totalSec << "s]";
        }
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
    state.isPlaying = m_isPlaying && g_ctx.initialized && !m_isPaused;
    state.isPaused = m_isPaused;
    state.volume = m_volume;
    
    if (m_isPlaying && g_ctx.initialized) {
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
    
    // Define max buffer size (e.g., 20 chunks ~ 20MB)
    const size_t MAX_BUFFER_CHUNKS = 20;

    while (!m_stopSignal) {
        // Flow Control: Block if buffer is full
        {
            std::unique_lock<std::mutex> lock(m_bufferMutex);
            m_bufferCV.wait(lock, [this, MAX_BUFFER_CHUNKS]() { 
                return m_rollingBuffer.size() < MAX_BUFFER_CHUNKS || m_stopSignal || m_seekRequested;
            });
        }
        
        if (m_stopSignal) break;

        size_t currentChunk = Abby::AbbyCrypt::getCurrentChunk();
        
        // Decrypt next chunk if available
        if (currentChunk < m_totalChunks) {
            std::vector<unsigned char> newChunk;
            try {
                newChunk = Abby::AbbyCrypt::decryptNextChunk();
            } catch (const std::exception& e) {
                std::cerr << "[AudioPlayer] Decryption Error: " << e.what() << std::endl;
                m_lastError = "Decryption Failed";
                m_stopSignal = true; // Fatal error
                m_bufferCV.notify_all(); // Wake up reader to stop
                break;
            }
            
            if (!newChunk.empty()) {
                std::lock_guard<std::mutex> lock(m_bufferMutex);
                
                // Add new chunk
                AudioChunk ac;
                ac.data = std::move(newChunk);
                ac.chunkIndex = currentChunk;
                m_rollingBuffer.push_back(std::move(ac));
                
                m_bufferCV.notify_all(); // Notify reader
                
                // Debug log occasionally
                if (m_rollingBuffer.size() % 5 == 0) {
                     std::cout << "[AudioPlayer] Buffered " << m_rollingBuffer.size() << " chunks. Next: " << currentChunk + 1 << std::endl;
                }
            }
        } else {
            // End of File reached, just wait
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    std::cout << "[AudioPlayer] Decryption thread stopped" << std::endl;
}
