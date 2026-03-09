import asyncio
import logging
from bleak import BleakScanner, BleakClient

# --- Configuration ---
REMOTE_ADDR = "45:3C:C1:BF:57:5A"
SENSOR_ADDR = "33:53:F9:85:68:94"

SERVICE_ID_REMOTE = "19b10000-e8f2-537e-4f6c-d104768a1214"
CHAR_ID_REMOTE_PRESS = "19b10001-e8f2-537e-4f6c-d104768a1214"
CHAR_ID_STATUS_UPDATE = "19b10002-e8f2-537e-4f6c-d104768a1214"

SERVICE_ID_SENSOR = "12345678-1234-5678-1234-56789abcdef0"
CHAR_ID_SENSOR_TRIGGER = "abcdef01-1234-5678-1234-56789abcdef0"

# --- Global State ---
class AlarmSystem:
    def __init__(self):
        self.is_active = False  # Sensor connected
        self.is_armed = False   # Armed via remote
        self.alarm_on = False   # Triggered
        self.remote_client = None

    async def broadcast_state(self):
        if self.remote_client and self.remote_client.is_connected:
            # Data: [is_active, is_armed, alarm_on]
            payload = bytes([int(self.is_active), int(self.is_armed), int(self.alarm_on)])
            try:
                await self.remote_client.write_gatt_char(CHAR_ID_STATUS_UPDATE, payload, response=True)
                print(f"Broadcasted: Active={self.is_active}, Armed={self.is_armed}, Alarm={self.alarm_on}")
            except Exception as e:
                print(f"Failed to broadcast: {e}")

    def update_actuator(self):
        status = "ON" if self.alarm_on else "OFF"
        print(f"--- ALARM ACTUATOR: {status} ---")

alarm = AlarmSystem()

# --- Handlers ---
def sensor_notification_handler(sender, data):
    # C++ logic: if data[0] == 0xFF
    if data[0] == 0xFF:
        if alarm.is_active and alarm.is_armed:
            alarm.alarm_on = True
            print("SENSOR TRIGGERED!")
            alarm.update_actuator()
            asyncio.create_task(alarm.broadcast_state())

def remote_notification_handler(sender, data):
    if data[0] == 0xFF:
        if not alarm.is_active:
            alarm.is_armed = False
            alarm.alarm_on = False
        else:
            if alarm.is_armed:
                alarm.is_armed = False
                alarm.alarm_on = False
            else:
                alarm.is_armed = True
        
        print(f"Remote toggle: Armed={alarm.is_armed}")
        alarm.update_actuator()
        asyncio.create_task(alarm.broadcast_state())

# --- Connection Tasks ---
async def manage_sensor():
    while True:
        print(f"Scanning for Sensor: {SENSOR_ADDR}")
        device = await BleakScanner.find_device_by_address(SENSOR_ADDR, timeout=10.0)
        if device:
            try:
                async with BleakClient(device) as client:
                    print("Sensor Connected")
                    alarm.is_active = True
                    await alarm.broadcast_state()
                    
                    await client.start_notify(CHAR_ID_SENSOR_TRIGGER, sensor_notification_handler)
                    
                    # Keep connection alive until disconnect
                    while client.is_connected:
                        await asyncio.sleep(1)
            except Exception as e:
                print(f"Sensor connection error: {e}")
        
        # Cleanup on disconnect
        alarm.is_active = False
        alarm.is_armed = False
        alarm.alarm_on = False
        print("Sensor disconnected. Retrying...")
        await alarm.broadcast_state()
        await asyncio.sleep(5)

async def manage_remote():
    while True:
        print(f"Scanning for Remote: {REMOTE_ADDR}")
        device = await BleakScanner.find_device_by_filter(
            lambda d, ad: d.name == REMOTE_ADDR
        )
        if device:
            try:
                async with BleakClient(device) as client:
                    print("Remote Connected")
                    alarm.remote_client = client
                    await alarm.broadcast_state()
                    
                    await client.start_notify(CHAR_ID_REMOTE_PRESS, remote_notification_handler)
                    
                    while client.is_connected:
                        await asyncio.sleep(1)
            except Exception as e:
                print(f"Remote connection error: {e}")
        
        alarm.remote_client = None
        print("Remote disconnected. Retrying...")
        await asyncio.sleep(5)

async def monitor_state():
    while True:
        state_active = "active" if alarm.is_active else "inactive"
        state_armed = "armed" if alarm.is_armed else "not armed"
        state_alarm = "ALARM!" if alarm.alarm_on else "quiet"
        print(f"Status: [{state_active}] [{state_armed}] [{state_alarm}]")
        await asyncio.sleep(2)

async def main():
    # Run all tasks concurrently
    await asyncio.gather(
        manage_sensor(),
        manage_remote(),
        monitor_state()
    )

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("System shut down.")