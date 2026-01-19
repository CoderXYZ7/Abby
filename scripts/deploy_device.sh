#!/bin/bash
#
# Device Deployment Script
# Configures AbbyPlayer for a specific device installation.
#
# Usage: sudo ./scripts/deploy_device.sh <device_name> <audio_path> [--skip-build]
#   or:  sudo ./scripts/deploy_device.sh (interactive)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
INSTALL_DIR="/opt/abby"
CONFIG_DIR="/etc/abby"
AUDIO_DIR="/var/lib/abby/audio"
SKIP_BUILD=false

echo "======================================"
echo "   AbbyPlayer Device Deployment Tool  "
echo "======================================"
echo ""

# Check root
if [ "$EUID" -ne 0 ]; then
    echo "Error: Please run as root (sudo ./scripts/deploy_device.sh)"
    exit 1
fi

# Parse arguments
DEVICE_NAME=""
SOURCE_AUDIO_DIR=""

for arg in "$@"; do
    if [ "$arg" = "--skip-build" ]; then
        SKIP_BUILD=true
    elif [ -z "$DEVICE_NAME" ]; then
        DEVICE_NAME="$arg"
    elif [ -z "$SOURCE_AUDIO_DIR" ]; then
        SOURCE_AUDIO_DIR="$arg"
    fi
done

# 1. Device Configuration - from args or interactive
if [ -n "$DEVICE_NAME" ] && [ -n "$SOURCE_AUDIO_DIR" ]; then
    echo "Device Name: $DEVICE_NAME"
    echo "Audio Path:  $SOURCE_AUDIO_DIR"
    echo "Skip Build:  $SKIP_BUILD"
else
    # Interactive mode
    echo "--- CONFIGURATION ---"
    read -p "Enter Device Name (Bluetooth Name) [AbbyConnector]: " DEVICE_NAME
    DEVICE_NAME=${DEVICE_NAME:-AbbyConnector}

    echo ""
    echo "Enter path to source audio files (mp3/wav/etc):"
    read -e -p "> " SOURCE_AUDIO_DIR
    
    read -p "Skip compilation? [y/N]: " SKIP_INPUT
    if [ "$SKIP_INPUT" = "y" ] || [ "$SKIP_INPUT" = "Y" ]; then
        SKIP_BUILD=true
    fi
fi

if [ ! -d "$SOURCE_AUDIO_DIR" ]; then
    echo "Error: Directory '$SOURCE_AUDIO_DIR' does not exist."
    exit 1
fi

echo ""
echo "Deploying as '$DEVICE_NAME' with audio from '$SOURCE_AUDIO_DIR'..."

# List source files for debugging
echo ""
echo "Source audio files:"
ls -la "$SOURCE_AUDIO_DIR"
echo ""

if [ "$SKIP_BUILD" = false ]; then
    # 2. Install Dependencies
    echo ""
    echo "--- INSTALLING DEPENDENCIES ---"
    apt-get update
    apt-get install -y \
        build-essential \
        cmake \
        pkg-config \
        libssl-dev \
        libsdl2-dev \
        libglew-dev \
        libbluetooth-dev \
        libdbus-1-dev \
        libsystemd-dev \
        libasound2-dev \
        bluez \
        bluetooth \
        python3-dbus \
        python3-gi

    echo "Dependencies installed."

    # Set make jobs to 1 for low-memory devices
    MAKE_JOBS=1

    # Create temporary swap if RAM is low (< 1GB free)
    FREE_MEM=$(free -m | awk '/Mem:/ {print $7}')
    if [ "$FREE_MEM" -lt 1024 ]; then
        echo "Low memory detected ($FREE_MEM MB). Creating temporary swap..."
        SWAP_FILE="/var/tmp/abby_build_swap"
        if [ ! -f "$SWAP_FILE" ]; then
            dd if=/dev/zero of="$SWAP_FILE" bs=1M count=1024 status=progress 2>/dev/null || dd if=/dev/zero of="$SWAP_FILE" bs=1M count=1024
            chmod 600 "$SWAP_FILE"
            mkswap "$SWAP_FILE"
        fi
        swapon "$SWAP_FILE" 2>/dev/null || true
        echo "Swap enabled."
    fi

    # Build Player (includes encrypt_util and crypt library)
    echo "Building AbbyPlayer..."
    rm -rf "$ROOT_DIR/player/build"
    rm -rf "$ROOT_DIR/client/build"
    rm -rf "$ROOT_DIR/crypt/build"
    mkdir -p "$ROOT_DIR/player/build"
    cd "$ROOT_DIR/player/build"
    cmake ..
    make -j$MAKE_JOBS
    cd "$ROOT_DIR"

    # Copy libabby-client.so to location expected by connector
    echo "Copying libabby-client.so..."
    mkdir -p "$ROOT_DIR/client/build"
    cp "$ROOT_DIR/player/build/abby-client/libabby-client.so" "$ROOT_DIR/client/build/"

    # Build Connector
    echo "Building AbbyConnector..."
    rm -rf "$ROOT_DIR/connector/build"
    mkdir -p "$ROOT_DIR/connector/build"
    cd "$ROOT_DIR/connector/build"
    cmake ..
    make -j$MAKE_JOBS
    cd "$ROOT_DIR"

    echo "All binaries built successfully."
else
    echo ""
    echo "--- SKIPPING BUILD (--skip-build) ---"
    
    # Check binaries exist
    if [ ! -f "$ROOT_DIR/player/build/AbbyPlayer" ]; then
        echo "ERROR: AbbyPlayer not found. Run without --skip-build first."
        exit 1
    fi
    if [ ! -f "$ROOT_DIR/connector/build/AbbyConnector" ]; then
        echo "ERROR: AbbyConnector not found. Run without --skip-build first."
        exit 1
    fi
    echo "Using existing binaries."
fi

# 4. Setup Environment (Install binaries and services)
echo ""
echo "--- SETTING UP ENVIRONMENT ---"

# Stop existing services first (to avoid "Text file busy" error)
echo "Stopping existing services..."
systemctl stop abby-player abby-connector bt-agent 2>/dev/null || true

# Create directories
mkdir -p "$INSTALL_DIR"
mkdir -p "$CONFIG_DIR"
mkdir -p "$AUDIO_DIR"

# Install freshly built binaries
echo "Installing binaries..."
cp "$ROOT_DIR/player/build/AbbyPlayer" "$INSTALL_DIR/"
cp "$ROOT_DIR/connector/build/AbbyConnector" "$INSTALL_DIR/"
cp "$ROOT_DIR/connector/scripts/bt_agent.py" "$INSTALL_DIR/"
chmod +x "$INSTALL_DIR/AbbyPlayer"
chmod +x "$INSTALL_DIR/AbbyConnector"
chmod +x "$INSTALL_DIR/bt_agent.py"

# Note: catalog.json will be generated later from encrypted audio files

# Install systemd services
cat > /etc/systemd/system/abby-player.service << 'EOF'
[Unit]
Description=AbbyPlayer Audio Daemon
After=sound.target

[Service]
Type=simple
ExecStart=/opt/abby/AbbyPlayer --daemon
WorkingDirectory=/opt/abby
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

cat > /etc/systemd/system/abby-connector.service << 'EOF'
[Unit]
Description=AbbyConnector Bluetooth Server
After=abby-player.service bluetooth.target
Wants=abby-player.service bluetooth.target

[Service]
Type=simple
ExecStart=/opt/abby/AbbyConnector --ble
WorkingDirectory=/opt/abby
Restart=always
RestartSec=5
Environment=CATALOG_PATH=/etc/abby/catalog.json

[Install]
WantedBy=multi-user.target
EOF

cat > /etc/systemd/system/bt-agent.service << 'EOF'
[Unit]
Description=Bluetooth Auto-Accept Agent
After=bluetooth.target
Wants=bluetooth.target

[Service]
Type=simple
ExecStart=/usr/bin/python3 /opt/abby/bt_agent.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

# Enable services
systemctl daemon-reload
systemctl enable bluetooth bt-agent abby-player abby-connector

echo "Environment setup complete."

# Update service with device name
SERVICE_FILE="/etc/systemd/system/abby-connector.service"
echo "Updating $SERVICE_FILE with Device Name: $DEVICE_NAME"

sed -i "s|ExecStart=.*|ExecStart=/opt/abby/AbbyConnector --ble --name \"$DEVICE_NAME\"|" "$SERVICE_FILE"

systemctl daemon-reload

# 4. Process Audio
echo ""
echo "--- PROCESSING AUDIO FILES ---"
echo "Encrypting files to $AUDIO_DIR..."
ENCRYPT_TOOL="$ROOT_DIR/player/build/encrypt_util"

# Clean entire audio directory (remove old encrypted AND unencrypted files)
rm -rf "$AUDIO_DIR"/*

COUNT=0
ERRORS=0

# Debug: Show what we're iterating over
echo "Scanning: $SOURCE_AUDIO_DIR/*"

for file in "$SOURCE_AUDIO_DIR"/*; do
    echo "Found: $file"
    if [ -f "$file" ]; then
        filename=$(basename "$file")
        ext="${filename##*.}"
        filename_noext="${filename%.*}"
        
        echo "  -> File: $filename, Extension: $ext"
        
        # Only encrypt audio files
        case "$ext" in
            mp3|wav|ogg|flac|aac|m4a)
                outfile="$AUDIO_DIR/${filename_noext}.pira"
                echo "Encrypting: $filename -> ${filename_noext}.pira"
                
                if "$ENCRYPT_TOOL" "$file" "$outfile"; then
                    COUNT=$((COUNT+1))
                else
                    echo "ERROR: Failed to encrypt $filename"
                    ERRORS=$((ERRORS+1))
                fi
                ;;
            *)
                echo "Skipping non-audio file: $filename"
                ;;
        esac
    fi
done

echo "Processed $COUNT files successfully ($ERRORS errors)."

# 5. Generate Catalog
echo ""
echo "--- UPDATING CATALOG ---"
python3 "$SCRIPT_DIR/generate_catalog.py" "$AUDIO_DIR" "$CONFIG_DIR/catalog.json"

# Show generated catalog
echo "Generated catalog:"
cat "$CONFIG_DIR/catalog.json"

# 6. Restart
echo ""
echo "--- RESTARTING SERVICES ---"
systemctl restart abby-player
systemctl restart abby-connector
systemctl restart bt-agent

echo ""
echo "======================================"
echo "   DEPLOYMENT COMPLETE!               "
echo "======================================"
echo "Device Name: $DEVICE_NAME"
echo "Track Count: $COUNT"
echo "Status:"
systemctl is-active abby-connector
echo ""
echo "Logs:"
echo "  journalctl -u abby-connector -f"
echo "======================================"
