#include "FileHandler.hpp"
#include "CryptoEngine.hpp"
#include <fstream>
#include <vector>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <algorithm>

// Static members
std::ifstream FileHandler::currentFile;
std::string FileHandler::currentSerial;
size_t FileHandler::totalChunks = 0;
size_t FileHandler::currentChunkIndex = 0;
std::vector<ChunkMetadata> FileHandler::chunkMetadata;

// PIRA v2 Format:
// [0-3]   Magic "PIRA"
// [4]     Version 0x02
// [5-8]   Total chunks (uint32_t)
// [9-12]  Chunk size (uint32_t)
//
// For each chunk:
// [0-11]  IV (12 bytes)
// [12-27] Tag (16 bytes)
// [28-N]  Encrypted chunk data

bool FileHandler::encryptFile(const std::string& sourcePath, const std::string& destPath, const std::string& serial) {
    // 1. Read Input
    std::ifstream inFile(sourcePath, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error: Could not open source file: " << sourcePath << std::endl;
        return false;
    }
    
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    inFile.close();
    
    if (data.empty()) {
        std::cerr << "Error: Source file is empty" << std::endl;
        return false;
    }
    
    // 2. Calculate chunks
    size_t numChunks = (data.size() + CHUNK_SIZE_BYTES - 1) / CHUNK_SIZE_BYTES;
    
    std::cout << "Encrypting " << data.size() << " bytes in " << numChunks << " chunks..." << std::endl;
    
    // 3. Derive key once
    std::vector<unsigned char> key = CryptoEngine::deriveKey(serial);
    
    // 4. Open output file
    std::ofstream outFile(destPath, std::ios::binary);
    if (!outFile) return false;
    
    // 5. Write header
    const char magic[] = "PIRA";
    outFile.write(magic, 4);
    
    char version = 0x02;
    outFile.write(&version, 1);
    
    uint32_t numChunksU32 = static_cast<uint32_t>(numChunks);
    uint32_t chunkSizeU32 = CHUNK_SIZE_BYTES;
    outFile.write(reinterpret_cast<const char*>(&numChunksU32), sizeof(uint32_t));
    outFile.write(reinterpret_cast<const char*>(&chunkSizeU32), sizeof(uint32_t));
    
    // 6. Encrypt and write each chunk
    for (size_t i = 0; i < numChunks; ++i) {
        size_t offset = i * CHUNK_SIZE_BYTES;
        size_t chunkSize = std::min(static_cast<size_t>(CHUNK_SIZE_BYTES), data.size() - offset);
        
        std::vector<unsigned char> chunk(data.begin() + offset, data.begin() + offset + chunkSize);
        
        std::vector<unsigned char> iv, tag;
        std::vector<unsigned char> encrypted = CryptoEngine::encrypt(chunk, key, iv, tag);
        
        if (encrypted.empty()) {
            std::cerr << "Error: Chunk " << i << " encryption failed" << std::endl;
            return false;
        }
        
        // Write chunk: IV + Tag + Data
        outFile.write(reinterpret_cast<const char*>(iv.data()), iv.size());
        outFile.write(reinterpret_cast<const char*>(tag.data()), tag.size());
        outFile.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
    }
    
    outFile.close();
    std::cout << "Encryption complete: " << numChunks << " chunks written" << std::endl;
    return true;
}

bool FileHandler::openEncryptedFile(const std::string& sourcePath, const std::string& serial) {
    closeEncryptedFile();
    
    currentFile.open(sourcePath, std::ios::binary);
    if (!currentFile) return false;
    
    currentSerial = serial;
    currentChunkIndex = 0;
    chunkMetadata.clear();
    
    // Read header
    char magic[4];
    currentFile.read(magic, 4);
    if (std::memcmp(magic, "PIRA", 4) != 0) {
        std::cerr << "Error: Invalid magic bytes" << std::endl;
        closeEncryptedFile();
        return false;
    }
    
    char version;
    currentFile.read(&version, 1);
    if (version != 0x02) {
        std::cerr << "Error: Unsupported version (expected v2)" << std::endl;
        closeEncryptedFile();
        return false;
    }
    
    uint32_t numChunksU32, chunkSizeU32;
    currentFile.read(reinterpret_cast<char*>(&numChunksU32), sizeof(uint32_t));
    currentFile.read(reinterpret_cast<char*>(&chunkSizeU32), sizeof(uint32_t));
    
    totalChunks = numChunksU32;
    
    std::cout << "[FileHandler] Opened PIRA v2: " << totalChunks << " chunks" << std::endl;
    return true;
}

std::vector<unsigned char> FileHandler::decryptNextChunk() {
    if (!currentFile.is_open() || currentChunkIndex >= totalChunks) {
        return {};
    }
    
    // Read chunk metadata
    std::vector<unsigned char> iv(12);
    std::vector<unsigned char> tag(16);
    
    currentFile.read(reinterpret_cast<char*>(iv.data()), 12);
    currentFile.read(reinterpret_cast<char*>(tag.data()), 16);
    
    if (!currentFile.good()) {
        std::cerr << "[FileHandler] Failed to read chunk metadata" << std::endl;
        return {};
    }
    
    // Read encrypted chunk data
    // For chunked GCM, encrypted size = plaintext size (GCM doesn't add padding)
    size_t chunkSize = (currentChunkIndex < totalChunks - 1) ? CHUNK_SIZE_BYTES : CHUNK_SIZE_BYTES;
    std::vector<unsigned char> encryptedChunk(chunkSize);
    
    currentFile.read(reinterpret_cast<char*>(encryptedChunk.data()), chunkSize);
    size_t bytesRead = currentFile.gcount();
    
    if (bytesRead == 0) {
        std::cerr << "[FileHandler] No data read for chunk " << currentChunkIndex << std::endl;
        return {};
    }
    
    // Resize to actual bytes read
    encryptedChunk.resize(bytesRead);
    
    // Decrypt
    std::vector<unsigned char> key = CryptoEngine::deriveKey(currentSerial);
    std::vector<unsigned char> decrypted = CryptoEngine::decrypt(encryptedChunk, key, iv, tag);
    
    currentChunkIndex++;
    return decrypted;
}

void FileHandler::closeEncryptedFile() {
    if (currentFile.is_open()) {
        currentFile.close();
    }
    currentSerial.clear();
    totalChunks = 0;
    currentChunkIndex = 0;
    chunkMetadata.clear();
}

size_t FileHandler::getTotalChunks() {
    return totalChunks;
}

size_t FileHandler::getCurrentChunk() {
    return currentChunkIndex;
}

// Compatibility: Decrypt entire file to memory
std::vector<unsigned char> FileHandler::decryptToMemory(const std::string& sourcePath, const std::string& serial) {
    if (!openEncryptedFile(sourcePath, serial)) {
        return {};
    }
    
    std::vector<unsigned char> fullData;
    
    while (currentChunkIndex < totalChunks) {
        std::vector<unsigned char> chunk = decryptNextChunk();
        if (chunk.empty()) break;
        
        fullData.insert(fullData.end(), chunk.begin(), chunk.end());
    }
    
    closeEncryptedFile();
    
    std::cout << "[FileHandler] Decrypted " << fullData.size() << " bytes total" << std::endl;
    return fullData;
}
