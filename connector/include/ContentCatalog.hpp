#pragma once

#include <string>
#include <map>
#include <vector>
#include "json.hpp"

class ContentCatalog {
public:
    struct TrackInfo {
        std::string path;
        std::string title;
        std::string requiredPermission;
    };

    ContentCatalog();
    bool load(const std::string& path);
    
    // Returns true if found, struct filled
    bool resolve(const std::string& code, TrackInfo& outInfo);
    
    std::vector<std::string> getTrackCodes() const;
    
    // Serialize catalog to JSON
    std::string toJson() const;

private:
    std::map<std::string, TrackInfo> m_tracks;
};
