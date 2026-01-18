#include "PlaylistManager.hpp"
#include <algorithm>
#include <random>
#include <sstream>

PlaylistManager::PlaylistManager() = default;

void PlaylistManager::addTrack(const std::string& trackId) {
    m_tracks.push_back(trackId);
    regenerateShuffleOrder();
}

void PlaylistManager::removeTrack(size_t index) {
    if (index >= m_tracks.size()) return;
    
    m_tracks.erase(m_tracks.begin() + index);
    if (m_currentIndex >= m_tracks.size() && !m_tracks.empty()) {
        m_currentIndex = m_tracks.size() - 1;
    }
    regenerateShuffleOrder();
}

void PlaylistManager::clearPlaylist() {
    m_tracks.clear();
    m_shuffleOrder.clear();
    m_currentIndex = 0;
}

void PlaylistManager::shuffle() {
    regenerateShuffleOrder();
}

std::string PlaylistManager::getCurrentTrack() const {
    if (m_tracks.empty()) return "";
    
    size_t idx = m_shuffleEnabled ? m_shuffleOrder[m_currentIndex] : m_currentIndex;
    return m_tracks[idx];
}

std::string PlaylistManager::getNextTrack() {
    if (m_tracks.empty()) return "";
    
    if (m_repeatMode == RepeatMode::ONE) {
        return getCurrentTrack();
    }
    
    if (m_currentIndex + 1 < m_tracks.size()) {
        m_currentIndex++;
    } else if (m_repeatMode == RepeatMode::ALL) {
        m_currentIndex = 0;
        if (m_shuffleEnabled) regenerateShuffleOrder();
    } else {
        return ""; // End of playlist
    }
    
    return getCurrentTrack();
}

std::string PlaylistManager::getPreviousTrack() {
    if (m_tracks.empty()) return "";
    
    if (m_currentIndex > 0) {
        m_currentIndex--;
    } else if (m_repeatMode == RepeatMode::ALL) {
        m_currentIndex = m_tracks.size() - 1;
    }
    
    return getCurrentTrack();
}

bool PlaylistManager::hasNext() const {
    if (m_tracks.empty()) return false;
    if (m_repeatMode == RepeatMode::ONE || m_repeatMode == RepeatMode::ALL) return true;
    return m_currentIndex + 1 < m_tracks.size();
}

bool PlaylistManager::hasPrevious() const {
    if (m_tracks.empty()) return false;
    if (m_repeatMode == RepeatMode::ALL) return true;
    return m_currentIndex > 0;
}

std::vector<std::string> PlaylistManager::getPlaylist() const {
    if (!m_shuffleEnabled) return m_tracks;
    
    std::vector<std::string> shuffled;
    for (size_t idx : m_shuffleOrder) {
        shuffled.push_back(m_tracks[idx]);
    }
    return shuffled;
}

size_t PlaylistManager::getCurrentIndex() const {
    return m_currentIndex;
}

size_t PlaylistManager::size() const {
    return m_tracks.size();
}

void PlaylistManager::setRepeatMode(RepeatMode mode) {
    m_repeatMode = mode;
}

PlaylistManager::RepeatMode PlaylistManager::getRepeatMode() const {
    return m_repeatMode;
}

void PlaylistManager::setShuffleEnabled(bool enabled) {
    m_shuffleEnabled = enabled;
    if (enabled) regenerateShuffleOrder();
}

bool PlaylistManager::isShuffleEnabled() const {
    return m_shuffleEnabled;
}

std::string PlaylistManager::toJson() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"currentIndex\":" << m_currentIndex << ",";
    ss << "\"currentTrack\":\"" << getCurrentTrack() << "\",";
    ss << "\"size\":" << m_tracks.size() << ",";
    ss << "\"repeat\":\"";
    switch (m_repeatMode) {
        case RepeatMode::NONE: ss << "none"; break;
        case RepeatMode::ONE: ss << "one"; break;
        case RepeatMode::ALL: ss << "all"; break;
    }
    ss << "\",";
    ss << "\"shuffle\":" << (m_shuffleEnabled ? "true" : "false") << ",";
    ss << "\"tracks\":[";
    for (size_t i = 0; i < m_tracks.size(); i++) {
        if (i > 0) ss << ",";
        ss << "\"" << m_tracks[i] << "\"";
    }
    ss << "]}";
    return ss.str();
}

void PlaylistManager::regenerateShuffleOrder() {
    m_shuffleOrder.clear();
    for (size_t i = 0; i < m_tracks.size(); i++) {
        m_shuffleOrder.push_back(i);
    }
    
    if (m_shuffleEnabled && m_shuffleOrder.size() > 1) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(m_shuffleOrder.begin(), m_shuffleOrder.end(), g);
    }
}
