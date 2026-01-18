#include "ContentCatalog.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

ContentCatalog::ContentCatalog() {}

bool ContentCatalog::load(const std::string& path) {
    try {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        
        json j;
        f >> j;
        
        if (j.contains("tracks")) {
            for (auto& item : j["tracks"]) {
                if (!item.contains("id")) continue;
                std::string code = item["id"];
                TrackInfo ti;
                ti.path = item["path"];
                if (item.contains("title")) ti.title = item["title"];
                if (item.contains("required_permission")) {
                    ti.requiredPermission = item["required_permission"];
                }
                m_tracks[code] = ti;
            }
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load catalog: " << e.what() << std::endl;
        return false;
    }
}

bool ContentCatalog::resolve(const std::string& code, TrackInfo& outInfo) {
    auto it = m_tracks.find(code);
    if (it != m_tracks.end()) {
        outInfo = it->second;
        return true;
    }
    return false;
}

std::vector<std::string> ContentCatalog::getTrackCodes() const {
    std::vector<std::string> codes;
    for (const auto& kv : m_tracks) {
        codes.push_back(kv.first);
    }
    return codes;
}

std::string ContentCatalog::toJson() const {
    std::ostringstream ss;
    ss << "{\"tracks\":[";
    bool first = true;
    for (const auto& kv : m_tracks) {
        if (!first) ss << ",";
        first = false;
        ss << "{\"id\":\"" << kv.first << "\",";
        ss << "\"title\":\"" << kv.second.title << "\",";
        ss << "\"path\":\"" << kv.second.path << "\",";
        ss << "\"permission\":\"" << kv.second.requiredPermission << "\"}";
    }
    ss << "]}";
    return ss.str();
}
