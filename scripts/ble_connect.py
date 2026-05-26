#!/usr/bin/env python3
"""
BLE connection script for ESP32-C6-LCD dev board.
Scans for device named "ESP32-C6-LCD", connects, and holds the connection.
"""

import asyncio
import sys
from bleak import BleakScanner, BleakClient

TARGET_NAME = "ESP32-C6-LCD"
SERVICE_UUID = "0000a000-0000-1000-8000-00805f9b34fb"
CHAR_UUID = "0000a001-0000-1000-8000-00805f9b34fb"


async def main():
    print(f"Scanning for '{TARGET_NAME}'...")

    device = None
    devices = await BleakScanner.discover(return_adv=True)

    for d, adv_data in devices.values():
        if adv_data.local_name == TARGET_NAME or (d.name and d.name == TARGET_NAME):
            device = d
            break

    if device is None:
        print(f"Device '{TARGET_NAME}' not found.")
        sys.exit(1)

    print(f"Found: {device.name} [{device.address}]")
    print("Connecting...")

    async with BleakClient(device) as client:
        if not client.is_connected:
            print("Failed to connect.")
            sys.exit(1)

        print(f"Connected to {device.name} [{device.address}]")

        try:
            value = await client.read_gatt_char(CHAR_UUID)
            print(f"Characteristic value: {value.hex()}")
        except Exception as e:
            print(f"Could not read characteristic: {e}")

        print("Holding connection. Press Ctrl+C to disconnect.")
        try:
            while client.is_connected:
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            print("\nDisconnecting...")

    print("Disconnected.")


if __name__ == "__main__":
    asyncio.run(main())
