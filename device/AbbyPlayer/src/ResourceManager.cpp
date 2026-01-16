#include "ResourceManager.hpp"
#include <cstdlib>
#include <unistd.h>
#include <linux/limits.h>
#include <iostream>

namespace fs = std::filesystem;

namespace Abby {

ResourceManager& ResourceManager::instance() {
    static ResourceManager inst;
    return inst;
}

ResourceManager::ResourceManager() {
    // Get executable directory
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len > 0) {
        exePath[len] = '\0';
        m_executableDir = fs::path(exePath).parent_path().string();
    }

    // Build search paths (order matters)
    // 1. Environment override
    const char* envPath = std::getenv("ABBY_RESOURCE_PATH");
    if (envPath && fs::exists(envPath)) {
        m_searchPaths.push_back(envPath);
    }

    // 2. Relative to executable (for development)
    if (!m_executableDir.empty()) {
        m_searchPaths.push_back(m_executableDir);
        // Also check parent of build dir (common dev layout: build/AbbyPlayer, assets at project root)
        m_searchPaths.push_back((fs::path(m_executableDir) / "..").string());
        m_searchPaths.push_back((fs::path(m_executableDir) / "../..").string());
    }

    // 3. System paths
    m_searchPaths.push_back("/usr/share/abby");
    m_searchPaths.push_back("/usr/local/share/abby");

    // 4. User config
    const char* home = std::getenv("HOME");
    if (home) {
        m_searchPaths.push_back(std::string(home) + "/.config/abby");
        m_searchPaths.push_back(std::string(home) + "/.local/share/abby");
    }
}

void ResourceManager::setOverridePath(const std::string& path) {
    m_overridePath = path;
}

std::string ResourceManager::findDirectory(const std::string& relativeName) const {
    // Check override first
    if (!m_overridePath.empty()) {
        fs::path p = fs::path(m_overridePath) / relativeName;
        if (fs::is_directory(p)) {
            return fs::canonical(p).string();
        }
    }

    // Search all paths
    for (const auto& base : m_searchPaths) {
        fs::path p = fs::path(base) / relativeName;
        if (fs::is_directory(p)) {
            return fs::canonical(p).string();
        }
    }

    std::cerr << "[ResourceManager] WARNING: Directory not found: " << relativeName << std::endl;
    return "";
}

std::string ResourceManager::findFile(const std::string& relativeName) const {
    // Check override first
    if (!m_overridePath.empty()) {
        fs::path p = fs::path(m_overridePath) / relativeName;
        if (fs::is_regular_file(p)) {
            return fs::canonical(p).string();
        }
    }

    // Search all paths
    for (const auto& base : m_searchPaths) {
        fs::path p = fs::path(base) / relativeName;
        if (fs::is_regular_file(p)) {
            return fs::canonical(p).string();
        }
    }

    std::cerr << "[ResourceManager] WARNING: File not found: " << relativeName << std::endl;
    return "";
}

std::string ResourceManager::getExecutableDir() const {
    return m_executableDir;
}

std::string ResourceManager::getUserConfigDir() const {
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.config/abby";
    }
    return "";
}

} // namespace Abby
