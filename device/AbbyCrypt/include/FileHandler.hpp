#pragma once
#include <vector>
#include <string>
#include <cstdint>

// PIRA v2 Format - Chunked streaming encryption
// Chunk size: 1 second @ 44.1kHz stereo 16-bit = ~176KB
#define CHUNK_SIZE_BYTES 176400  // 1 second of audio

struct ChunkMetadata {
    std::vector<unsigned char> iv;   // 12 bytes
    std::vector<unsigned char> tag;  // 16 bytes
    size_t dataSize;                  // actual chunk data size
};

class FileHandler {
public:
    // PIRA v2: Chunked encryption for streaming
    static bool encryptFile(const std::string& sourcePath, const std::string& destPath, const std::string& serial);
    
    // Streaming decryption
    static bool openEncryptedFile(const std::string& sourcePath, const std::string& serial);
    static std::vector<unsigned char> decryptNextChunk();
    static void closeEncryptedFile();
    static size_t getTotalChunks();
    static size_t getCurrentChunk();
    static void seekToChunk(size_t chunkIndex);
    
    // Legacy: Decrypt entire file to memory (for compatibility during transition)
    static std::vector<unsigned char> decryptToMemory(const std::string& sourcePath, const std::string& serial);
    
private:
    static std::ifstream currentFile;
    static std::string currentSerial;
    static size_t totalChunks;
    static size_t currentChunkIndex;
    static uint32_t storedChunkSize;
    static std::vector<ChunkMetadata> chunkMetadata;
};
