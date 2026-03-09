import asyncio
from bleak import BleakClient

# Vaihda tähän Nano 33 BLE Sense:n MAC-osoite
address = "33:53:F9:85:68:94"

# Motion Characteristic UUID, sama kuin Arduino-koodissa
motion_uuid = "abcdef01-1234-5678-1234-56789abcdef0"

def handle_motion(sender, data):
    if data[0] == 1:
        print("Motion detected!")
    else:
        print("No motion")

async def run(address):
    async with BleakClient(address) as client:
        print("Connected to Nano!")
        await client.start_notify(motion_uuid, handle_motion)

        while True:
            await asyncio.sleep(1)

asyncio.run(run(address))
