import asyncio
import time
from bleak import BleakScanner, BleakClient
import threading
import tracemalloc

# --- Configuration ---
REMOTE_ADDR = "45:3C:C1:BF:57:5A"
SENSOR_ADDR = "33:53:F9:85:68:94"

# UUIDs
SERVICE_ID_REMOTE = "19b10000-e8f2-537e-4f6c-d104768a1214"
CHAR_ID_REMOTE_PRESS = "19b10001-e8f2-537e-4f6c-d104768a1214"
CHAR_ID_STATUS_UPDATE = "19b10002-e8f2-537e-4f6c-d104768a1214"

SERVICE_ID_SENSOR = "12345678-1234-5678-1234-56789abcdef0"
CHAR_ID_SENSOR_TRIGGER = "abcdef01-1234-5678-1234-56789abcdef0"

# --- State Management ---
class BurglarySystem:
    def __init__(self):
        self.is_active = False  # Sensor connected
        self.is_armed = False   # Armed state
        self.alarm_on = False   # Alarm trigger
        self.remote_client = None
        self.lock = asyncio.Lock() # Prevents simultaneous DBus commands

    async def update_remote_status(self):
        """Writes [active, armed, alarm] to the remote."""
        if self.remote_client and self.remote_client.is_connected:
            payload = bytes([int(self.is_active), int(self.is_armed), int(self.alarm_on)])
            try:
                await self.remote_client.write_gatt_char(CHAR_ID_STATUS_UPDATE, payload)
                print(f"Sent status to Remote: {list(payload)}")
            except Exception as e:
                print(f"Status broadcast failed: {e}")

alarm = BurglarySystem()

# --- Callbacks ---
def on_sensor_data(sender, data):
    if data[0] == 0xFF and alarm.is_active and alarm.is_armed:
        alarm.alarm_on = True
        print("!!! ALARM TRIGGERED BY SENSOR !!!")
        asyncio.create_task(alarm.update_remote_status())

def on_remote_press(sender, data):
    if data[0] == 0xFF:
        if not alarm.is_active:
            alarm.is_armed = False
            alarm.alarm_on = False
        else:
            alarm.is_armed = not alarm.is_armed
            if not alarm.is_armed:
                alarm.alarm_on = False
        
        print(f"Remote Toggle -> Armed: {alarm.is_armed}, Alarm: {alarm.alarm_on}")
        asyncio.create_task(alarm.update_remote_status())

# --- Connection Manager ---
async def manage_device(address, name):
    """Handles connection, notification setup, and reconnection for one device."""
    while True:

        async with alarm.lock: # Ensure we don't collide during connection attempts
            print(f"Scanning for {name} ({address})...")
            device = await BleakScanner.find_device_by_address(address, timeout=5.0)
            
            if not device:
                await asyncio.sleep(2)
                continue

        try:
            async with BleakClient(device) as client:
                print(f"Connected to {name}")
                
                if address == SENSOR_ADDR:
                    alarm.is_active = True
                    await client.start_notify(CHAR_ID_SENSOR_TRIGGER, on_sensor_data)
                else:
                    alarm.remote_client = client
                    await client.start_notify(CHAR_ID_REMOTE_PRESS, on_remote_press)
                
                await alarm.update_remote_status()
                
                while client.is_connected:
                    await asyncio.sleep(1)

        except Exception as e:
            print(f"Error in {name} loop: {e}")
        finally:
            if address == SENSOR_ADDR:
                alarm.is_active = False
                alarm.is_armed = False
                alarm.alarm_on = False
            else:
                alarm.remote_client = None
            
            print(f"{name} disconnected. Re-scanning...")
            await alarm.update_remote_status()
            await asyncio.sleep(2)

def sync_wrapper(addr, name):
    return asyncio.run(manage_device(addr, name))

async def main():

    await asyncio.to_thread(sync_wrapper, REMOTE_ADDR, "REMOTE")

    await manage_device(SENSOR_ADDR, "SENSOR")
        

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nSystem Disarmed. Exiting.")