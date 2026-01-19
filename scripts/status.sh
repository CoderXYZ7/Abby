#!/bin/bash
#
# AbbyPlayer System Status
#

echo "=== Abby System Status ==="
echo ""

echo "--- Services ---"
for svc in abby-player abby-connector bt-agent bluetooth; do
    status=$(systemctl is-active $svc 2>/dev/null || echo "not found")
    printf "%-20s %s\n" "$svc:" "$status"
done

echo ""
echo "--- Bluetooth ---"
if command -v hciconfig &>/dev/null; then
    hciconfig hci0 2>/dev/null | head -3 || echo "No Bluetooth adapter"
fi

echo ""
echo "--- Audio Catalog ---"
if [ -f /etc/abby/catalog.json ]; then
    tracks=$(python3 -c "import json; print(len(json.load(open('/etc/abby/catalog.json'))['tracks']))" 2>/dev/null || echo "?")
    echo "Tracks: $tracks"
else
    echo "Catalog: Not found"
fi

echo ""
echo "--- Recent Logs ---"
journalctl -u abby-connector -n 5 --no-pager 2>/dev/null | tail -5 || echo "No logs"
