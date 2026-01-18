#pragma once

#include <string>
#include <vector>
#include <functional>

/**
 * Playlist Manager for AbbyConnector
 * Manages track queues and playback order
 */

struct PlaylistTrack {
    std::string trackId;
    std::string title;
    int durationSec;
};

class PlaylistManager {
public:
    PlaylistManager();
    
    // Queue management
    void addTrack(const std::string& trackId);
    void removeTrack(size_t index);
    void clearPlaylist();
    void shuffle();
    
    // Playback control
    std::string getCurrentTrack() const;
    std::string getNextTrack();
    std::string getPreviousTrack();
    bool hasNext() const;
    bool hasPrevious() const;
    
    // Playlist info
    std::vector<std::string> getPlaylist() const;
    size_t getCurrentIndex() const;
    size_t size() const;
    
    // Repeat/shuffle modes
    enum class RepeatMode { NONE, ONE, ALL };
    void setRepeatMode(RepeatMode mode);
    RepeatMode getRepeatMode() const;
    
    void setShuffleEnabled(bool enabled);
    bool isShuffleEnabled() const;
    
    // Serialize for status
    std::string toJson() const;

private:
    std::vector<std::string> m_tracks;
    std::vector<size_t> m_shuffleOrder;
    size_t m_currentIndex = 0;
    RepeatMode m_repeatMode = RepeatMode::NONE;
    bool m_shuffleEnabled = false;
    
    void regenerateShuffleOrder();
};
