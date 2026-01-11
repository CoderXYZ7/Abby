#include "AbbyCrypt.hpp"
#include "FileHandler.hpp"
#include "HardwareID.hpp"

namespace Abby {

std::vector<unsigned char> AbbyCrypt::decryptTrackToMemory(const std::string& piraPath) {
    return FileHandler::decryptToMemory(piraPath, HardwareID::getSerial());
}

bool AbbyCrypt::encryptTrackFile(const std::string& inputPath, const std::string& outputPath, const std::string& targetSerial) {
    return FileHandler::encryptFile(inputPath, outputPath, targetSerial);
}

std::string AbbyCrypt::getHardwareSerial() {
    return HardwareID::getSerial();
}

bool AbbyCrypt::openEncryptedFile(const std::string& path, const std::string& serial) {
    return FileHandler::openEncryptedFile(path, serial);
}

std::vector<unsigned char> AbbyCrypt::decryptNextChunk() {
    return FileHandler::decryptNextChunk();
}

void AbbyCrypt::closeEncryptedFile() {
    FileHandler::closeEncryptedFile();
}

size_t AbbyCrypt::getTotalChunks() {
    return FileHandler::getTotalChunks();
}

size_t AbbyCrypt::getCurrentChunk() {
    return FileHandler::getCurrentChunk();
}

}
