import asyncio
import threading
from bleak import BleakClient
import tkinter as tk

device_address = "FF:EF:D3:39:90:86"  # Replace with your device MAC address
NUS_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

lux_value = 0

def handle_lux(_, data):
    global lux_value
    try:
        lux_value = int(data.decode())
    except Exception as e:
        print(f"Decode error: {e}")

async def run_ble():
    async with BleakClient(device_address) as client:
        await client.start_notify(NUS_UUID, handle_lux)
        while True:
            await asyncio.sleep(1)  # Keep loop alive

def start_ble_loop():
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.run_until_complete(run_ble())

def correct_lux(lux_input):
    a = 1.14052902 * pow(10, -13)
    b = -2.07339275 * pow(10, -9)
    c = 9.38846304 * pow(10, -5)
    d = 5.89559631 * pow(10, -1)
    e = 1081.22474
    #=$J$4*POW(D2,4) + $K$4*POW(D2,3) + $L$4*POW(D2,2) + $M$4*POW(D2,1)+$N$4
    output_lux = (a * pow(lux_input, 4)) + (b * pow(lux_input, 3)) + (c * pow(lux_input, 2)) + (d * pow(lux_input, 1)) + e
    return int(output_lux)

# GUI setup
root = tk.Tk()
root.title("Lux Readings Raw")
root.geometry("300x150")

label = tk.Label(root, text="Waiting for lux...", font=("Helvetica", 64))
label.pack(expand=True)

def update_gui():
    label.config(text=f"Raw Lux: {lux_value}")
    root.after(250, update_gui)  # Schedule next update in 250 ms

# Start BLE in background thread
ble_thread = threading.Thread(target=start_ble_loop, daemon=True)
ble_thread.start()

update_gui()  # Start GUI update loop
root.mainloop()