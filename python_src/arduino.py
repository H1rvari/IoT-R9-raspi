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
        # Track active tasks to prevent duplicate management
        self.active_sessions = {"SENSOR": False, "REMOTE": False}

    async def broadcast(self):
        if self.remote_client and self.remote_client.is_connected:
            try:
                payload = bytes([int(self.is_active), int(self.is_armed), int(self.alarm_on)])
                await self.remote_client.write_gatt_char(CHAR_ID_STATUS_UPDATE, payload)
                print(f"-> Broadcast: Active={self.is_active}, Armed={self.is_armed}, Alarm={self.alarm_on}")
            except Exception:
                pass

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
        print(f"Remote Press: Armed={alarm.is_armed}")
        asyncio.create_task(alarm.broadcast())

# --- The "Manager": Handles Data and Notifications ---
async def manage_device(client, name):
    """Once connected, this runs as a background task."""
    try:
        alarm.active_sessions[name] = True
        if name == "SENSOR":
            alarm.is_active = True
            await client.start_notify(CHAR_ID_SENSOR_TRIGGER, sensor_callback)
        else:
            alarm.remote_client = client
            await client.start_notify(CHAR_ID_REMOTE_PRESS, remote_callback)
        
        await alarm.broadcast()

        # Keep the connection alive
        while client.is_connected:
            await asyncio.sleep(1)

    finally:
        # Cleanup
        alarm.active_sessions[name] = False
        if name == "SENSOR":
            alarm.is_active = alarm.is_armed = alarm.alarm_on = False
        else:
            alarm.remote_client = None
        print(f"[{name}] Disconnected.")
        await alarm.broadcast()

# --- The "Connector": Runs Sequentially ---
async def main_loop():
    print("Starting Sequential Connection Manager...")
    
    while True:
        # 1. Check Sensor
        if not alarm.active_sessions["SENSOR"]:
            print("[SENSOR] Searching...")
            device = await BleakScanner.find_device_by_address(SENSOR_ADDR, timeout=5.0)
            if device:
                print("[SENSOR] Found. Connecting...")
                try:
                    # We enter a context manager here, but we'll use a Future 
                    # to keep the client open while the loop continues
                    client = BleakClient(device)
                    await client.connect()
                    # Kick off the management as a background task
                    asyncio.create_task(manage_device(client, "SENSOR"))
                    print("[SENSOR] Managed. Moving to next device...")
                except Exception as e:
                    print(f"[SENSOR] Connection failed: {e}")

        # 2. Check Remote
        if not alarm.active_sessions["REMOTE"]:
            print("[REMOTE] Searching...")
            device = await BleakScanner.find_device_by_address(REMOTE_ADDR, timeout=5.0)
            if device:
                print("[REMOTE] Found. Connecting...")
                try:
                    client = BleakClient(device)
                    await client.connect()
                    asyncio.create_task(manage_device(client, "REMOTE"))
                    print("[REMOTE] Managed.")
                except Exception as e:
                    print(f"[REMOTE] Connection failed: {e}")

        # Short pause before the next check of the "Connector"
        await asyncio.sleep(2)

if __name__ == "__main__":
    try:
        asyncio.run(main_loop())
    except KeyboardInterrupt:
        print("Shutdown.")