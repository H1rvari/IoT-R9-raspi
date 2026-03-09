import asyncio
from bleak import BleakScanner, BleakClient

# Configuration
REMOTE_ADDR = "45:3C:C1:BF:57:5A"
SENSOR_ADDR = "33:53:F9:85:68:94"

# UUIDs
CHAR_ID_REMOTE_PRESS = "19b10001-e8f2-537e-4f6c-d104768a1214"
CHAR_ID_STATUS_UPDATE = "19b10002-e8f2-537e-4f6c-d104768a1214"
CHAR_ID_SENSOR_TRIGGER = "abcdef01-1234-5678-1234-56789abcdef0"

class AlarmSystem:
    def __init__(self):
        self.is_active = False
        self.is_armed = False
        self.alarm_on = False
        self.remote_client = None
        self.connected_devices = set()
        self.lock = asyncio.Lock()

    async def broadcast(self):
        if self.remote_client and self.remote_client.is_connected:
            try:
                payload = bytes([int(self.is_active), int(self.is_armed), int(self.alarm_on)])
                await self.remote_client.write_gatt_char(CHAR_ID_STATUS_UPDATE, payload)
                print(f"Broadcast -> Active:{self.is_active} Armed:{self.is_armed} Alarm:{self.alarm_on}")
            except Exception as e:
                print(f"Broadcast failed: {e}")

alarm = AlarmSystem()

# --- Callbacks ---
def sensor_callback(sender, data):
    if data[0] == 0xFF and alarm.is_active and alarm.is_armed:
        alarm.alarm_on = True
        print("!!! ALARM TRIGGERED !!!")
        asyncio.create_task(alarm.broadcast())

def remote_callback(sender, data):
    if data[0] == 0xFF:
        if not alarm.is_active:
            alarm.is_armed, alarm.alarm_on = False, False
        else:
            alarm.is_armed = not alarm.is_armed
            if not alarm.is_armed: alarm.alarm_on = False
        print(f"Remote Press -> Armed: {alarm.is_armed}")
        asyncio.create_task(alarm.broadcast())

# --- Connection Logic ---
async def connect_to_device(device):
    """Handles the connection once a device is spotted by the scanner."""
    addr = device.address.upper()
    if addr in alarm.connected_devices:
        return

    async with alarm.lock:  # BlueZ on Pi hates simultaneous connection attempts
        alarm.connected_devices.add(addr)
        print(f"Attempting to connect to {addr}...")
        
        try:
            async with BleakClient(device) as client:
                print(f"Successfully connected to {addr}")
                
                if addr == SENSOR_ADDR:
                    alarm.is_active = True
                    await client.start_notify(CHAR_ID_SENSOR_TRIGGER, sensor_callback)
                elif addr == REMOTE_ADDR:
                    alarm.remote_client = client
                    await client.start_notify(CHAR_ID_REMOTE_PRESS, remote_callback)
                
                await alarm.broadcast()

                # Loop until disconnect
                while client.is_connected:
                    await asyncio.sleep(1)
                    
        except Exception as e:
            print(f"Connection to {addr} failed: {e}")
        finally:
            alarm.connected_devices.remove(addr)
            if addr == SENSOR_ADDR:
                alarm.is_active = False
                alarm.is_armed = False
                alarm.alarm_on = False
            elif addr == REMOTE_ADDR:
                alarm.remote_client = None
            print(f"Disconnected from {addr}")
            await alarm.broadcast()

# --- Scanner Loop ---
async def run_scanner():
    """Single scanner that triggers connection tasks."""
    print("Starting Raspberry Pi BLE Scanner...")
    
    def detection_callback(device, advertisement_data):
        addr = device.address.upper()
        if addr in [REMOTE_ADDR, SENSOR_ADDR] and addr not in alarm.connected_devices:
            # Kick off a connection task without blocking the scanner
            asyncio.create_task(connect_to_device(device))

    # Scanner runs continuously
    async with BleakScanner(detection_callback=detection_callback):
        while True:
            await asyncio.sleep(5)
            # Periodic status print
            print(f"Scanning... Connected: {list(alarm.connected_devices)}")

async def main():
    try:
        await run_scanner()
    except Exception as e:
        print(f"Main loop error: {e}")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nShutting down Alarm System.")