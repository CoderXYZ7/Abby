#!/usr/bin/env python3
"""
Bluetooth Auto-Accept Agent for Headless Devices
Automatically accepts all pairing and connection requests without user interaction.
Run as: sudo python3 bt_agent.py
"""

import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib
import sys

AGENT_PATH = "/org/bluez/AutoAgent"

class AutoAcceptAgent(dbus.service.Object):
    """
    Bluetooth agent that auto-accepts all pairing and authorization requests.
    Capability: NoInputNoOutput (Just Works pairing)
    """
    
    def __init__(self, bus, path):
        super().__init__(bus, path)
        print("[Agent] Initialized at", path)

    @dbus.service.method("org.bluez.Agent1", in_signature="", out_signature="")
    def Release(self):
        print("[Agent] Released")

    @dbus.service.method("org.bluez.Agent1", in_signature="os", out_signature="")
    def AuthorizeService(self, device, uuid):
        print(f"[Agent] AuthorizeService: {device} UUID={uuid}")
        # Auto-authorize all services
        return

    @dbus.service.method("org.bluez.Agent1", in_signature="o", out_signature="s")
    def RequestPinCode(self, device):
        print(f"[Agent] RequestPinCode: {device}")
        return "0000"

    @dbus.service.method("org.bluez.Agent1", in_signature="o", out_signature="u")
    def RequestPasskey(self, device):
        print(f"[Agent] RequestPasskey: {device}")
        return dbus.UInt32(0)

    @dbus.service.method("org.bluez.Agent1", in_signature="ouq", out_signature="")
    def DisplayPasskey(self, device, passkey, entered):
        print(f"[Agent] DisplayPasskey: {device} passkey={passkey}")

    @dbus.service.method("org.bluez.Agent1", in_signature="os", out_signature="")
    def DisplayPinCode(self, device, pincode):
        print(f"[Agent] DisplayPinCode: {device} pin={pincode}")

    @dbus.service.method("org.bluez.Agent1", in_signature="ou", out_signature="")
    def RequestConfirmation(self, device, passkey):
        print(f"[Agent] RequestConfirmation: {device} passkey={passkey} -> AUTO-ACCEPTING")
        # Auto-confirm all pairing requests
        return

    @dbus.service.method("org.bluez.Agent1", in_signature="o", out_signature="")
    def RequestAuthorization(self, device):
        print(f"[Agent] RequestAuthorization: {device} -> AUTO-ACCEPTING")
        # Auto-authorize all connection requests
        return

    @dbus.service.method("org.bluez.Agent1", in_signature="", out_signature="")
    def Cancel(self):
        print("[Agent] Cancelled")


def main():
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()
    
    # Create agent
    agent = AutoAcceptAgent(bus, AGENT_PATH)
    
    # Register with BlueZ
    manager = dbus.Interface(
        bus.get_object("org.bluez", "/org/bluez"),
        "org.bluez.AgentManager1"
    )
    
    try:
        manager.RegisterAgent(AGENT_PATH, "NoInputNoOutput")
        print("[Agent] Registered with capability: NoInputNoOutput")
        
        manager.RequestDefaultAgent(AGENT_PATH)
        print("[Agent] Set as default agent")
    except dbus.exceptions.DBusException as e:
        print(f"[Agent] Error: {e}")
        sys.exit(1)
    
    print("[Agent] Running... (Ctrl+C to stop)")
    print("[Agent] All pairing and connection requests will be auto-accepted")
    
    try:
        GLib.MainLoop().run()
    except KeyboardInterrupt:
        print("\n[Agent] Shutting down...")
        manager.UnregisterAgent(AGENT_PATH)


if __name__ == "__main__":
    main()
