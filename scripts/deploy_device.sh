#!/bin/bash
#
# Interactive Device Deployment Script
# Configures AbbyPlayer for a specific device installation.
#
# Usage: sudo ./scripts/deploy_device.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
INSTALL_DIR="/opt/abby"
CONFIG_DIR="/etc/abby"
AUDIO_DIR="/var/lib/abby/audio"

echo "======================================"
echo "   AbbyPlayer Device Deployment Tool  "
echo "======================================"
echo ""

# Check root
if [ "$EUID" -ne 0 ]; then
    echo "Error: Please run as root (sudo ./scripts/deploy_device.sh)"
    exit 1
fi

# 1. Device Configuration
echo "--- CONFIGURATION ---"
read -p "Enter Device Name (Bluetooth Name) [AbbyConnector]: " DEVICE_NAME
DEVICE_NAME=${DEVICE_NAME:-AbbyConnector}

echo ""
echo "Enter path to source audio files (mp3/wav/etc):"
read -e -p "> " SOURCE_AUDIO_DIR

if [ ! -d "$SOURCE_AUDIO_DIR" ]; then
    echo "Error: Directory '$SOURCE_AUDIO_DIR' does not exist."
    exit 1
fi

echo ""
echo "Deploying as '$DEVICE_NAME' with audio from '$SOURCE_AUDIO_DIR'..."
echo "Press Enter to continue or Ctrl+C to cancel."
read

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
    libsystemd-dev \
    python3 \
    python3-pip \
    bluetooth \
    bluez \
    || { echo "Failed to install dependencies"; exit 1; }

echo "Dependencies installed."

# 3. Build ALL Binaries (clean build for correct architecture)
echo ""
echo "--- BUILDING ALL BINARIES ---"
echo "Building all components for this architecture (this may take 5-10 minutes)..."

# Use -j1 to avoid OOM on low-memory devices (RPi Zero, etc.)
MAKE_JOBS=1

# Build Player (includes encrypt_util and crypt library)
echo "Building AbbyPlayer..."
rm -rf "$ROOT_DIR/player/build"
mkdir -p "$ROOT_DIR/player/build"
cd "$ROOT_DIR/player/build"
cmake ..
make -j$MAKE_JOBS
cd "$ROOT_DIR"

# Build Connector
echo "Building AbbyConnector..."
rm -rf "$ROOT_DIR/connector/build"
mkdir -p "$ROOT_DIR/connector/build"
cd "$ROOT_DIR/connector/build"
cmake ..
make -j$MAKE_JOBS
cd "$ROOT_DIR"

echo "All binaries built successfully."

# 4. Setup Environment (Install binaries and services)
echo ""
echo "--- SETTING UP ENVIRONMENT ---"

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

# Install config
cp "$ROOT_DIR/connector/config/catalog.json" "$CONFIG_DIR/"

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

# Configure Bluetooth compat mode
mkdir -p /etc/systemd/system/bluetooth.service.d
cat > /etc/systemd/system/bluetooth.service.d/compat.conf << 'EOF'
[Service]
ExecStart=
ExecStart=/usr/libexec/bluetooth/bluetoothd --compat
EOF

# Enable services
systemctl daemon-reload
systemctl enable bluetooth bt-agent abby-player abby-connector

echo "Environment setup complete."

# Update service with device name
SERVICE_FILE="/etc/systemd/system/abby-connector.service"
echo "Updating $SERVICE_FILE with Device Name: $DEVICE_NAME"

# We use sed to replaceExecStart line
# Match: ExecStart=/opt/abby/AbbyConnector --ble ...
# Replace with: ExecStart=/opt/abby/AbbyConnector --ble --name "DEVICE_NAME"
sed -i "s|ExecStart=.*|ExecStart=/opt/abby/AbbyConnector --ble --name \"$DEVICE_NAME\"|" "$SERVICE_FILE"

systemctl daemon-reload

# 4. Process Audio
echo ""
echo "--- PROCESSING AUDIO FILES ---"
echo "Encrypting files to $AUDIO_DIR..."
ENCRYPT_TOOL="$ROOT_DIR/player/build/encrypt_util"

# Clean old audio
rm -f "$AUDIO_DIR"/*.pira

COUNT=0
for file in "$SOURCE_AUDIO_DIR"/*; do
    if [ -f "$file" ]; then
        filename=$(basename "$file")
        filename_noext="${filename%.*}"
        outfile="$AUDIO_DIR/${filename_noext}.pira"
        
        echo "Encrypting: $filename -> ${filename_noext}.pira"
        "$ENCRYPT_TOOL" "$file" "$outfile"
        ((COUNT++))
    fi
done

echo "Processed $COUNT files."

# 5. Generate Catalog
echo ""
echo "--- UPDATING CATALOG ---"
python3 "$SCRIPT_DIR/generate_catalog.py" "$AUDIO_DIR" "$CONFIG_DIR/catalog.json"

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
