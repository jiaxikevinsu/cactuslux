import asyncio
from bleak import BleakScanner, BleakClient
from rich.live import Live
from rich.panel import Panel
from rich.text import Text

#lux_measurement_char = '00002a37-0000-1000-8000-00805f9b34fb'
device_address = "FF:EF:D3:39:90:86"
lux_measurement_char = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

lux_value = 0 #global

def handle_lux(_, data):
    global lux_value
    try:
        lux_value = int(data.decode())
        #print(f"Lux received: {lux_value}")
    except Exception as e:
        print(f"Error decoding lux: {e}")

async def main():
    myDevice = ''
    devices = await BleakScanner.discover(5.0, return_adv=True)
    for d in devices:
        print(devices[d])
        if(devices[d][1].local_name == "Cactus Lux Sensor"):
            print("Found it")
            myDevice = d

    stop_event = asyncio.Event()

    address = myDevice
    async with BleakClient(address) as client:
        # svcs = await client.get_services()
        # for service in svcs:
        #     print(f"[Service] {service}")
        #     for char in service.characteristics:
        #         print(f"  [Characteristic] {char} (UUID: {char.uuid})")
        
        await client.start_notify(lux_measurement_char, handle_lux)

        with Live(refresh_per_second=4) as live:
            while True:
                display = Panel(Text(f"🌞 Lux: {lux_value}", style="bold magenta"), title="Live Sensor Data")
                live.update(display)
                await asyncio.sleep(0.25)

        # try:
        #     await stop_event.wait()  # waits forever until externally cancelled
        # except KeyboardInterrupt:
        #     print("\nStopping...")

        # await client.stop_notify(lux_measurement_char)

asyncio.run(main())