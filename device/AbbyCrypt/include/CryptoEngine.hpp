#pragma once
#include <vector>
#include <string>

class CryptoEngine {
public:
    static std::vector<unsigned char> deriveKey(const std::string& serial);
    static std::vector<unsigned char> encrypt(const std::vector<unsigned char>& plaintext, const std::vector<unsigned char>& key, std::vector<unsigned char>& outIV, std::vector<unsigned char>& outTag);
    static std::vector<unsigned char> decrypt(const std::vector<unsigned char>& ciphertext, const std::vector<unsigned char>& key, const std::vector<unsigned char>& iv, const std::vector<unsigned char>& tag);
};
