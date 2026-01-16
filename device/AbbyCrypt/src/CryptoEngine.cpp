#include "CryptoEngine.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <iostream>

std::vector<unsigned char> CryptoEngine::deriveKey(const std::string& serial) {
    std::vector<unsigned char> key(32);
    
    // Obfuscated Salt: "PIRAMID_SALT_2024" (18 bytes incl null)
    // XOR Key: 0x55 (arbitrary)
    // "P" (0x50) ^ 0x55 = 0x05
    // "I" (0x49) ^ 0x55 = 0x1C
    // ...
    // Generated at runtime to defeat 'strings'
    const unsigned char xorKey = 0x55;
    const unsigned char obfuscated[] = {
        0x50^0x55, 0x49^0x55, 0x52^0x55, 0x41^0x55, 0x4D^0x55, 0x49^0x55, 0x44^0x55, // PIRAMID
        0x5F^0x55, // _
        0x53^0x55, 0x41^0x55, 0x4C^0x55, 0x54^0x55, // SALT
        0x5F^0x55, // _
        0x32^0x55, 0x30^0x55, 0x32^0x55, 0x34^0x55, // 2024
        0x00^0x55  // Null terminator
    };
    
    unsigned char salt[sizeof(obfuscated)];
    for(size_t i=0; i<sizeof(obfuscated); i++) {
        salt[i] = obfuscated[i] ^ xorKey;
    }
    
    if (PKCS5_PBKDF2_HMAC(serial.c_str(), serial.length(), salt, sizeof(salt), 10000, EVP_sha256(), 32, key.data()) != 1) {
        std::cerr << "Error in key derivation" << std::endl;
        return {};
    }
    return key;
}

std::vector<unsigned char> CryptoEngine::encrypt(const std::vector<unsigned char>& plaintext, const std::vector<unsigned char>& key, std::vector<unsigned char>& outIV, std::vector<unsigned char>& outTag) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len;
    std::vector<unsigned char> ciphertext(plaintext.size());

    outIV.resize(12);
    if (RAND_bytes(outIV.data(), 12) != 1) return {};

    if(!(ctx = EVP_CIPHER_CTX_new())) return {};

    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) return {};

    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL)) return {};

    if(1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key.data(), outIV.data())) return {};

    if(1 != EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), plaintext.size())) return {};
    ciphertext_len = len;

    if(1 != EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len)) return {};
    ciphertext_len += len;

    outTag.resize(16);
    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, outTag.data())) return {};

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext;
}

std::vector<unsigned char> CryptoEngine::decrypt(const std::vector<unsigned char>& ciphertext, const std::vector<unsigned char>& key, const std::vector<unsigned char>& iv, const std::vector<unsigned char>& tag) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    std::vector<unsigned char> plaintext(ciphertext.size());

    if(!(ctx = EVP_CIPHER_CTX_new())) return {};

    if(!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) return {};

    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL)) return {};

    if(!EVP_DecryptInit_ex(ctx, NULL, NULL, key.data(), iv.data())) return {};

    if(1 != EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size())) return {};
    plaintext_len = len;

    if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void*)tag.data())) return {};

    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);

    if(ret > 0) {
        plaintext_len += len;
        plaintext.resize(plaintext_len);
        return plaintext;
    } else {
        return {};
    }
}
