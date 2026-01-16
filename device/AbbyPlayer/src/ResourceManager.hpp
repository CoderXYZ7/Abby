#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace Abby {

/**
 * ResourceManager - Resolves paths for assets (shaders, config, etc.)
 * 
 * Search order:
 * 1. Environment variable override (ABBY_RESOURCE_PATH)
 * 2. Path relative to executable
 * 3. System paths (/usr/share/abby, /usr/local/share/abby)
 * 4. User config (~/.config/abby)
 */
class ResourceManager {
public:
    static ResourceManager& instance();

    // Find a directory (e.g., "shaders")
    std::string findDirectory(const std::string& relativeName) const;
    
    // Find a file (e.g., "config/defaults.conf")
    std::string findFile(const std::string& relativeName) const;

    // Get standard paths
    std::string getExecutableDir() const;
    std::string getUserConfigDir() const;

    // Allow override (e.g., from command line)
    void setOverridePath(const std::string& path);

private:
    ResourceManager();
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    std::vector<std::string> m_searchPaths;
    std::string m_overridePath;
    std::string m_executableDir;
};

} // namespace Abby
