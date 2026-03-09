import asyncio
import logging
from bleak import BleakScanner, BleakClient

# --- Configuration ---
REMOTE_NAME = "BurglaryRemote"
SENSOR_ADDR = "33:53:F9:85:68:94"

SERVICE_ID_REMOTE = "19b10000-e8f2-537e-4f6c-d104768a1214"
CHAR_ID_REMOTE_PRESS = "19b10001-e8f2-537e-4f6c-d104768a1214"
CHAR_ID_STATUS_UPDATE = "19b10002-e8f2-537e-4f6c-d104768a1214"

SERVICE_ID_SENSOR = "12345678-1234-5678-1234-56789abcdef0"
CHAR_ID_SENSOR_TRIGGER = "abcdef01-1234-5678-1234-56789abcdef0"

# --- Global State ---
class SystemState:
    def __init__(self):
        self.is_active = False  # Sensor connected
        self.is_armed = False   # Armed by remote
        self.alarm_on = False   # Triggered
        self.remote_client = None

    def display(self):
        active = "Active" if self.is_active else "Inactive"
        armed = "Armed" if self.is_armed else "Disarmed"
        alarm = "ALARM ON" if self.alarm_on else "Quiet"
        print(f"[{active}] | [{armed}] | [{alarm}]")

state = SystemState()

async def broadcast_state():
    """Updates the Remote device with the current system status."""
    if state.remote_client and state.remote_client.is_connected:
        # Data format: [is_active, is_armed, alarm_on]
        data = bytearray([int(state.is_active), int(state.is_armed), int(state.alarm_on)])
        try:
            await state.remote_client.write_gatt_char(CHAR_ID_STATUS_UPDATE, data)
            print("Status broadcasted to remote.")
        except Exception as e:
            print(f"Failed to broadcast: {e}")

def update_actuator():
    print(f"---> Actuator Update: Alarm is {'ON' if state.alarm_on else 'OFF'}")

# --- Event Handlers ---

async def remote_notification_handler(sender, data):
    """Handles button presses from the remote."""
    print(f"Remote button pressed. Raw: {data.hex()}")
    
    if not state.is_active:
        state.is_armed = False
        state.alarm_on = False
    else:
        if state.is_armed:
            state.is_armed = False
            state.alarm_on = False
        else:
            state.is_armed = True
    
    update_actuator()
    await broadcast_state()

async def sensor_notification_handler(sender, data):
    """Handles movement/trigger from the sensor."""
    print(f"Sensor triggered. Raw: {data.hex()}")
    if state.is_active and state.is_armed:
        state.alarm_on = True
        update_actuator()
        await broadcast_state()

# --- Connection Managers ---

async def manage_remote():
    while True:
        print("Searching for Remote...")
        device = await BleakScanner.find_device_by_filter(
            lambda d, ad: d.name == REMOTE_NAME
        )
        
        if device:
            try:
                async with BleakClient(device) as client:
                    print(f"Connected to Remote: {device.address}")
                    state.remote_client = client
                    await client.start_notify(CHAR_ID_REMOTE_PRESS, remote_notification_handler)
                    await broadcast_state()
                    
                    # Keep connection alive until disconnect
                    while client.is_connected:
                        await asyncio.sleep(1)
            except Exception as e:
                print(f"Remote connection error: {e}")
        
        state.remote_client = None
        print("Remote disconnected. Retrying in 5s...")
        await asyncio.sleep(5)

async def manage_sensor():
    while True:
        print("Searching for Sensor...")
        device = await BleakScanner.find_device_by_address(SENSOR_ADDR)
        
        if device:
            try:
                async with BleakClient(device) as client:
                    print(f"Connected to Sensor: {device.address}")
                    state.is_active = True
                    await client.start_notify(CHAR_ID_SENSOR_TRIGGER, sensor_notification_handler)
                    
                    while client.is_connected:
                        await asyncio.sleep(1)
            except Exception as e:
                print(f"Sensor connection error: {e}")
        
        # Reset state on disconnect
        state.is_active = False
        state.is_armed = False
        state.alarm_on = False
        print("Sensor disconnected. System Disarmed. Retrying...")
        await broadcast_state()
        await asyncio.sleep(5)

async def main():
    # Run both connection managers concurrently
    await asyncio.gather(
        manage_remote(),
        manage_sensor(),
    )

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("System shut down.")