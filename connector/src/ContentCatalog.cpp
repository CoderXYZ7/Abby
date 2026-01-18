#include "ContentCatalog.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

ContentCatalog::ContentCatalog() {}

bool ContentCatalog::load(const std::string& path) {
    try {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        
        json j;
        f >> j;
        
        if (j.contains("tracks")) {
            for (auto& [code, info] : j["tracks"].items()) {
                TrackInfo ti;
                ti.path = info["path"];
                if (info.contains("required_permission")) {
                    ti.requiredPermission = info["required_permission"];
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
