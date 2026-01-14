#include "JwtValidator.hpp"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <time.h>

using json = nlohmann::json;

JwtValidator::JwtValidator(const std::string& publicKeyPath) : m_publicKeyPath(publicKeyPath) {}

JwtValidator::~JwtValidator() {}

std::vector<std::string> JwtValidator::split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string JwtValidator::base64UrlDecode(const std::string& input) {
    std::string base64 = input;
    std::replace(base64.begin(), base64.end(), '-', '+');
    std::replace(base64.begin(), base64.end(), '_', '/');
    while (base64.length() % 4) {
        base64 += '=';
    }
    
    // Basic base64 decode (manual or library - implementing simple one for std::string)
    // Actually, let's use a simple lookup or OpenSSL's BIO if needed, but manual is fine for small strings
    // Using OpenSSL BIO for robustness
    BIO *bio, *b64;
    int decodeLen = base64.length() * 3 / 4;
    char *buffer = (char *)malloc(base64.length() + 1);
    
    bio = BIO_new_mem_buf(base64.c_str(), -1);
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Do not require newlines
    int len = BIO_read(bio, buffer, base64.length());
    
    std::string result(buffer, len);
    free(buffer);
    BIO_free_all(bio);
    
    return result;
}

bool JwtValidator::verifySignature(const std::string& headerPayload, const std::string& signature) {
    EVP_PKEY* pubKey = nullptr;
    BIO* bio = BIO_new_file(m_publicKeyPath.c_str(), "r");
    if (!bio) {
        std::cerr << "Failed to load public key file: " << m_publicKeyPath << std::endl;
        return false;
    }
    
    pubKey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    BIO_free(bio);
    
    if (!pubKey) {
        std::cerr << "Failed to parse public key" << std::endl;
        return false;
    }
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_sha256();
    
    bool result = false;
    
    if (EVP_DigestVerifyInit(ctx, NULL, md, NULL, pubKey) > 0) {
        if (EVP_DigestVerifyUpdate(ctx, headerPayload.c_str(), headerPayload.length()) > 0) {
            std::string decodedSig = base64UrlDecode(signature);
            if (EVP_DigestVerifyFinal(ctx, (const unsigned char*)decodedSig.c_str(), decodedSig.length()) == 1) {
                result = true;
            }
        }
    }
    
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pubKey);
    return result;
}

JwtValidator::ValidationResult JwtValidator::validate(const std::string& token) {
    std::vector<std::string> parts = split(token, '.');
    if (parts.size() != 3) {
        return {false, "Invalid token format (not 3 parts)", {}};
    }
    
    std::string headerPayload = parts[0] + "." + parts[1];
    std::string signature = parts[2];
    
    if (!verifySignature(headerPayload, signature)) {
        return {false, "Invalid signature", {}};
    }
    
    // Decode payload
    try {
        std::string payloadJson = base64UrlDecode(parts[1]);
        json payload = json::parse(payloadJson);
        
        // Check expiration
        if (payload.contains("exp")) {
            long exp = payload["exp"];
            time_t now;
            time(&now);
            
            if (now > exp) {
                return {false, "Token expired", payload};
            }
        }
        
        return {true, "Valid", payload};
        
    } catch (const std::exception& e) {
        return {false, std::string("JSON parsing error: ") + e.what(), {}};
    }
}
