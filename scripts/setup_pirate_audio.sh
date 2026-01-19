#!/bin/bash
#
# Pirate Audio Setup Script for Raspberry Pi
# Configures /boot/config.txt for Pirate Audio DAC and installing UI dependencies
#
# Usage: sudo ./scripts/setup_pirate_audio.sh
#

set -e

if [ "$EUID" -ne 0 ]; then
    echo "Error: Please run as root (sudo ./scripts/setup_pirate_audio.sh)"
    exit 1
fi

CONFIG_TXT="/boot/config.txt"
# On some newer OSs (Bookworm), it might be /boot/firmware/config.txt
if [ -f "/boot/firmware/config.txt" ]; then
    CONFIG_TXT="/boot/firmware/config.txt"
fi

echo "=== Configuring Pirate Audio ==="
echo "Config file: $CONFIG_TXT"

# Backup
cp "$CONFIG_TXT" "${CONFIG_TXT}.backup"

# Add dtoverlay if not present
if ! grep -q "dtoverlay=hifiberry-dac" "$CONFIG_TXT"; then
    echo "Adding dtoverlay=hifiberry-dac..."
    echo "dtoverlay=hifiberry-dac" >> "$CONFIG_TXT"
fi

# Add gpio config
if ! grep -q "gpio=25=op,dh" "$CONFIG_TXT"; then
    echo "Adding gpio=25=op,dh..."
    echo "gpio=25=op,dh" >> "$CONFIG_TXT"
fi

# Disable onboard audio (optional but recommended)
# We comment out existing enable and add disable
sed -i 's/^dtparam=audio=on/#dtparam=audio=on/' "$CONFIG_TXT"
if ! grep -q "dtparam=audio=off" "$CONFIG_TXT"; then
    echo "Disabling onboard audio..."
    echo "dtparam=audio=off" >> "$CONFIG_TXT"
fi

# Install dependencies for UI (Display/Buttons)
echo "Installing Python dependencies for Display/Buttons..."
apt-get update
# Try installing via apt first (cleaner on RPi OS)
apt-get install -y python3-rpi.gpio python3-spidev python3-pil python3-numpy || true
# Install st7789 library
pip3 install st7789 --break-system-packages 2>/dev/null || pip3 install st7789

echo ""
echo "=== Pirate Audio Configuration Complete ==="
echo "A reboot is required for audio changes to take effect."
echo "Running 'sudo reboot' is recommended."
