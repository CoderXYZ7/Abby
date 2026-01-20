#!/bin/bash
#
# AbbyPlayer Headless Setup Script
# Installs and configures AbbyPlayer + AbbyConnector for headless operation
#
# Usage: sudo ./setup.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INSTALL_DIR="/opt/abby"
CONFIG_DIR="/etc/abby"
AUDIO_DIR="/var/lib/abby/audio"

echo "=== AbbyPlayer Headless Setup ==="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: Please run as root (sudo ./setup.sh)"
    exit 1
fi

# Detect source directories
if [ -d "$SCRIPT_DIR/../player/build" ]; then
    SOURCE_DIR="$SCRIPT_DIR/.."
else
    SOURCE_DIR="/home/maintainer/Dev/Piramid/abby"
fi

echo "[1/7] Creating directories..."
mkdir -p "$INSTALL_DIR"
mkdir -p "$CONFIG_DIR"
mkdir -p "$AUDIO_DIR"

echo "[2/7] Installing binaries..."
cp "$SOURCE_DIR/player/build/AbbyPlayer" "$INSTALL_DIR/"
cp "$SOURCE_DIR/connector/build/AbbyConnector" "$INSTALL_DIR/"
cp "$SOURCE_DIR/connector/scripts/bt_agent.py" "$INSTALL_DIR/"
chmod +x "$INSTALL_DIR/AbbyPlayer"
chmod +x "$INSTALL_DIR/AbbyConnector"
chmod +x "$INSTALL_DIR/bt_agent.py"

echo "[3/7] Installing configuration..."
cp "$SOURCE_DIR/connector/config/catalog.json" "$CONFIG_DIR/"

# Copy audio files if they exist
if [ -d "$SOURCE_DIR/audio" ]; then
    echo "    Copying audio files..."
    cp -r "$SOURCE_DIR/audio/"* "$AUDIO_DIR/" 2>/dev/null || true
fi

echo "[4/7] Installing systemd services..."

# AbbyPlayer service
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

# AbbyConnector service  
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

# Bluetooth Agent service
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

echo "[5/7] Configuring Bluetooth..."
# Ensure bluetoothd runs with compat mode for SDP
mkdir -p /etc/systemd/system/bluetooth.service.d
cat > /etc/systemd/system/bluetooth.service.d/compat.conf << 'EOF'
[Service]
ExecStart=
ExecStart=/usr/libexec/bluetooth/bluetoothd --compat
EOF

echo "[6/7] Enabling services..."
systemctl daemon-reload
systemctl enable bluetooth
systemctl enable bt-agent
systemctl enable abby-player
systemctl enable abby-connector

echo "[7/7] Starting services..."
# Unblock rfkill in case Bluetooth is soft-blocked
rfkill unblock bluetooth 2>/dev/null || true
systemctl restart bluetooth
sleep 2
# Ensure adapter is up
hciconfig hci0 up 2>/dev/null || true
systemctl start bt-agent
systemctl start abby-player
sleep 1
systemctl start abby-connector

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Services installed:"
echo "  - abby-player.service    (Audio daemon)"
echo "  - abby-connector.service (Bluetooth + TCP server)"
echo "  - bt-agent.service       (Auto-accept pairing)"
echo ""
echo "Configuration:"
echo "  - Binaries: $INSTALL_DIR"
echo "  - Config:   $CONFIG_DIR"
echo "  - Audio:    $AUDIO_DIR"
echo ""
echo "To check status:"
echo "  systemctl status abby-player"
echo "  systemctl status abby-connector"
echo "  systemctl status bt-agent"
echo ""
echo "The device is now ready for Bluetooth pairing!"
