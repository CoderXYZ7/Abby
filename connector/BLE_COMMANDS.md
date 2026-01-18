# AbbyConnector BLE Command Reference

This document lists all commands available via the BLE GATT interface and their expected responses.

## GATT Service

| Type | UUID |
|------|------|
| Service | `00001000-7072-6d64-5069-72616d696400` |
| Command (Write) | `00001001-7072-6d64-5069-72616d696400` |
| Response (Notify) | `00001002-7072-6d64-5069-72616d696400` |
| Status (Read/Notify) | `00001003-7072-6d64-5069-72616d696400` |

## Authentication

| Command | Description | Response |
|---------|-------------|----------|
| `AUTH <jwt>` | Authenticate with JWT token | `OK: Authenticated. Expires: <timestamp>` or `ERROR: <reason>` |

## Playback Control

| Command | Auth Required | Description | Response |
|---------|---------------|-------------|----------|
| `PLAY <track_id>` | ✓ | Play track by ID from catalog | `OK: Playing <id>` or `ERROR: <reason>` |
| `STOP` | ✗ | Stop playback | `OK` |
| `PAUSE` | ✗ | Pause playback | `OK` |
| `RESUME` | ✗ | Resume playback | `OK` |
| `VOLUME <0.0-1.0>` | ✗ | Set volume (0.0 = mute, 1.0 = max) | `OK: Volume <value>` |
| `STATUS` | ✗ | Get current playback status | `STOPPED`, `PLAYING`, or `PAUSED` |

## Playlist Management

| Command | Auth Required | Description | Response |
|---------|---------------|-------------|----------|
| `PLAYLIST_ADD <track_id>` | ✓ | Add track to end of playlist | `OK: Added <id> to playlist` |
| `PLAYLIST_REMOVE <index>` | ✗ | Remove track at index | `OK: Removed track at index <n>` |
| `PLAYLIST_CLEAR` | ✗ | Clear entire playlist | `OK: Playlist cleared` |
| `PLAYLIST_GET` | ✗ | Get playlist as JSON | `{"tracks":[...],"currentIndex":0,...}` |
| `PLAYLIST_NEXT` | ✓ | Play next track in playlist | Triggers `PLAY` for next track |
| `PLAYLIST_PREV` | ✓ | Play previous track in playlist | Triggers `PLAY` for previous track |
| `PLAYLIST_SHUFFLE on|off` | ✗ | Enable/disable shuffle mode | `OK: Shuffle enabled/disabled` |
| `PLAYLIST_REPEAT none|one|all` | ✗ | Set repeat mode | `OK: Repeat mode set to <mode>` |

## Catalog

| Command | Auth Required | Description | Response |
|---------|---------------|-------------|----------|
| `CATALOG_LIST` | ✗ | Get full track catalog as JSON | `{"tracks":[{"id":"...","title":"...","path":"..."},...]}` |

## Usage Examples

### Via BLE (Vict App)
1. Scan for device named "AbbyConnector"
2. Connect and discover services
3. Write to Command characteristic: `AUTH <jwt>`
4. Read Response characteristic for result
5. Write: `PLAY 1`
6. Subscribe to Status characteristic for updates

### Via Debug CLI
```bash
./AbbyConnector --debug
abby> AUTH eyJ0eXAiOiJKV1QiLCJhbGciOiJSUzI1NiJ9...
OK: Authenticated. Expires: 1771349328

abby> CATALOG_LIST
{"tracks":[{"id":"1","title":"Track 1",...}]}

abby> VOLUME 0.75
OK: Volume 0.75

abby> PLAY 1
OK: Playing 1

abby> STATUS
PLAYING

abby> STOP
OK
```

### Via TCP (port 5000)
```bash
echo "STATUS" | nc localhost 5000
```
