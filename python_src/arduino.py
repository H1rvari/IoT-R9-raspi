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
        self.is_active = False   # Sensor status
        self.is_armed = False    # Armed status
        self.alarm_on = False    # Trigger status
        self.remote_client = None
        self.lock = asyncio.Lock() # Crucial for Raspberry Pi BlueZ stability

    async def broadcast(self):
        """Updates the Remote with the current system state."""
        if self.remote_client and self.remote_client.is_connected:
            try:
                payload = bytes([int(self.is_active), int(self.is_armed), int(self.alarm_on)])
                await self.remote_client.write_gatt_char(CHAR_ID_STATUS_UPDATE, payload)
                print(f"-> Broadcast: Active={self.is_active}, Armed={self.is_armed}, Alarm={self.alarm_on}")
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
        
        print(f"Remote Press: Armed={alarm.is_armed}")
        asyncio.create_task(alarm.broadcast())

# --- Function 1: Managing the Session ---
async def manage_connection(client, name):
    """Handles notifications and keeps the session alive."""
    try:
        print(f"[{name}] Setting up notifications...")
        if name == "SENSOR":
            alarm.is_active = True
            await client.start_notify(CHAR_ID_SENSOR_TRIGGER, sensor_callback)
        else:
            alarm.remote_client = client
            await client.start_notify(CHAR_ID_REMOTE_PRESS, remote_callback)
        
        # Initial state sync
        await alarm.broadcast()

        # Keep alive loop
        while client.is_connected:
            await asyncio.sleep(1)
            
    except Exception as e:
        print(f"[{name}] Session error: {e}")
    finally:
        # Reset specific states on disconnect
        if name == "SENSOR":
            alarm.is_active = False
            alarm.is_armed = False
            alarm.alarm_on = False
        else:
            alarm.remote_client = None
        
        print(f"[{name}] Connection closed.")
        await alarm.broadcast()

# --- Function 2: The Reconnection Loop ---
async def connect_loop(address, name):
    """Handles discovery and the physical connection handshake."""
    print(f"Starting connection loop for {name}...")
    while True:
        try:
            # Discovery (Un-locked to allow both tasks to scan)
            device = await BleakScanner.find_device_by_address(address, timeout=10.0)
            if not device:
                await asyncio.sleep(2)
                continue

            # Connection (Locked to prevent Pi BlueZ 'InProgress' collision)
            async with alarm.lock:
                print(f"[{name}] Found! Handshaking...")
                async with BleakClient(device) as client:
                    # Pass the active client to the Manager function
                    await manage_connection(client, name)

        except Exception as e:
            print(f"[{name}] Connection failed: {e}")
        
        # Wait before retrying to prevent CPU spikes
        await asyncio.sleep(5)

async def main():
    # Schedule both loops
    await asyncio.gather(
        connect_loop(SENSOR_ADDR, "SENSOR"),
        connect_loop(REMOTE_ADDR, "REMOTE")
    )

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("System shutdown.")