#include "CryptoEngine.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <iostream>

std::vector<unsigned char> CryptoEngine::deriveKey(const std::string& serial) {
    std::vector<unsigned char> key(32);
    // Use PBKDF2 for key derivation
    // In production, we'd use a unique salt, but here we can use a project-fixed salt
    // or derive it from the serial itself.
    const unsigned char salt[] = "PIRAMID_SALT_2024";
    
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
