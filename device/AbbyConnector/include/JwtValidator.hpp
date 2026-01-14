#pragma once

#include <string>
#include <vector>
#include <map>
#include "json.hpp"

class JwtValidator {
public:
    JwtValidator(const std::string& publicKeyPath);
    ~JwtValidator();

    struct ValidationResult {
        bool valid;
        std::string error;
        nlohmann::json payload;
    };

    ValidationResult validate(const std::string& token);

private:
    std::string m_publicKeyPath;
    
    // Helper methods
    static std::vector<std::string> split(const std::string& s, char delimiter);
    static std::string base64UrlDecode(const std::string& input);
    bool verifySignature(const std::string& headerPayload, const std::string& signature);
};
