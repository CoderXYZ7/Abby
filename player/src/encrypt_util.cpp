#include <iostream>
#include <vector>
#include <string>
#include "AbbyCrypt.hpp"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: encrypt_util <input_file> <output_file> [hardware_id]\n";
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    std::string hardwareId;

    if (argc >= 4) {
        hardwareId = argv[3];
    } else {
        hardwareId = Abby::AbbyCrypt::getHardwareSerial();
        std::cout << "Using local Hardware ID: " << hardwareId << std::endl;
    }

    if (Abby::AbbyCrypt::encryptTrackFile(inputPath, outputPath, hardwareId)) {
        std::cout << "Successfully encrypted " << inputPath << " to " << outputPath << " for ID: " << hardwareId << std::endl;
    } else {
        std::cerr << "Failed to encrypt file." << std::endl;
        return 1;
    }

    return 0;
}
