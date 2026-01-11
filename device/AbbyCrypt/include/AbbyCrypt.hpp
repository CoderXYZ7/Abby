#pragma once
#include <vector>
#include <string>

namespace Abby {

class AbbyCrypt {
public:
    // Decrypt entire file to memory (legacy/compatibility)
    static std::vector<unsigned char> decryptTrackToMemory(const std::string& piraPath);
    
    // Encrypt a track file
    static bool encryptTrackFile(const std::string& inputPath, const std::string& outputPath, const std::string& targetSerial);
    
    // Get hardware serial
    static std::string getHardwareSerial();
    
    // Streaming API (for AudioPlayer)
    static bool openEncryptedFile(const std::string& path, const std::string& serial);
    static std::vector<unsigned char> decryptNextChunk();
    static void closeEncryptedFile();
    static size_t getTotalChunks();
    static size_t getCurrentChunk();
};

}
