import pyvista as pv
import pyvistaqt as pvqt
import numpy as np
import threading
import time
import csv
import os
import pandas as pd
from datetime import datetime
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

#create dataframe to store information to plot
column_names = ['datetime', 'lux', 'tempf', 'rh']
df = pd.DataFrame(columns=column_names)

#process data from csv
filename = os.path.join(os.path.dirname(__file__), "data", "data.csv")
cactus_file = os.path.join(os.path.dirname(__file__), "assets", "cactus_gasteria.obj")

def correct_lux(lux_input):
    a = 1.14052902 * pow(10, -13)
    b = -2.07339275 * pow(10, -9)
    c = 9.38846304 * pow(10, -5)
    d = 5.89559631 * pow(10, -1)
    e = 1081.22474
    #=$J$4*POW(D2,4) + $K$4*POW(D2,3) + $L$4*POW(D2,2) + $M$4*POW(D2,1)+$N$4
    output_lux = (a * pow(lux_input, 4)) + (b * pow(lux_input, 3)) + (c * pow(lux_input, 2)) + (d * pow(lux_input, 1)) + e
    return int(output_lux)

df = pd.read_csv(filename, names=column_names, header=None)
df['lux'] = df['lux'].astype(int)
df['tempf'] = df['tempf'].astype(float)
df['rh'] = df['rh'].astype(float)

df['datetime'] = pd.to_datetime(df['datetime'], format="%Y-%m-%d %H:%M:%S %a")
df['datetime2'] = df['datetime']
df['corrected_lux'] = df['lux'].apply(correct_lux)
df = df.set_index('datetime')

# Create the plot
fig, ax = plt.subplots(nrows=3, ncols=1, constrained_layout=True, sharex=True)

ax[0].plot(df.index, df['corrected_lux'], marker='o', linestyle='-', color='g')
ax[0].set_ylabel('lux', fontsize=30)
ax[0].tick_params(axis='y', labelsize=20)
ax[0].tick_params(axis="x", labelsize=20)
ax[0].grid(True)

ax[1].plot(df.index, df['tempf'], marker='o', linestyle=' ', color='r')
ax[1].set_ylabel('Fahrenheit', fontsize=30)
ax[1].tick_params(axis='y', labelsize=20)
ax[1].tick_params(axis="x", labelsize=20)
ax[1].set_ylim(0, 120)
ax[1].grid(True)

ax[2].plot(df.index, df['rh'], marker='o', linestyle=':', color='b')
# ax[2].set_title("Relative Humidity")
ax[2].set_ylabel('RH %', fontsize=30)
ax[2].tick_params(axis='y', labelsize=20)
ax[2].tick_params(axis="x", labelsize=20)
ax[2].grid(True)

plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%m-%d %H:%M %a'))
plt.gca().xaxis.set_major_locator(mdates.HourLocator(interval=1))
plt.xticks(fontsize=20)
plt.xticks(rotation=90)

print(df.head())

#Create 3D Part of the GUI
cactus_mesh = pv.read(cactus_file)
cactus_mesh.rotate_x(90, inplace=True)
sensor_output = {"lux": "", "date": ""}

#Create a callback for the slider
def slider_callback(input_intensity):
    light_top.intensity = (df['corrected_lux'][int(input_intensity)]) / 10000
    text = str((df['corrected_lux'][int(input_intensity)])) + " lux\n" + str(df['datetime2'][int(input_intensity)])
    text_actor.SetText(0, text)
    #date_text_actor.SetText(0, df['datetime'][int(input_intensity)])
    #print(df['lux'][int(input_intensity)])

light_top = pv.Light(position=(0, 30, 30), focal_point=(0, 0, 0), color=[255, 255, 255], intensity=0)#color=[235, 180, 52]
light_top.positional = True
# light_front = pv.Light(position=(0, 0, 1.0), focal_point=(0, 0, 0), color=[235, 180, 52], intensity=0)
# light_back = pv.Light(position=(0, 0, 1.0), focal_point=(0, 0, 0), color=[235, 180, 52], intensity=0)
# light_left = pv.Light(position=(0, 0, 1.0), focal_point=(0, 0, 0), color=[235, 180, 52], intensity=0)
# light_right = pv.Light(position=(0, 0, 1.0), focal_point=(0, 0, 0), color=[235, 180, 52], intensity=0)

# Create the background plotter
#plotter = pvqt.BackgroundPlotter(shape=(2,1), show=True)
plotter = pv.Plotter(shape=(2,1), lighting=None, window_size=(1920,2200))#, show=True)
plotter.background_color = "white"

# Create a sphere mesh
plotter.subplot(0,0)
plotter.add_chart(pv.ChartMPL(fig))

plotter.subplot(1, 0)
sphere = pv.Sphere(theta_resolution=30, phi_resolution=30)
cube = pv.Cube()
actor = plotter.add_mesh(cactus_mesh, color='green', show_edges=False)
plotter.add_light(light_top)
#plotter.enable_shadows()
plotter.add_slider_widget(slider_callback,rng=[0,len(df['corrected_lux'])], value=0, pointa=(.1, .9), pointb=(.9, .9), interaction_event="always")
text_actor = plotter.add_text(text="", position="lower_left", font_size=32)

#plotter.add_axes()
plotter.show()

#input("Press Enter to exit...\n")