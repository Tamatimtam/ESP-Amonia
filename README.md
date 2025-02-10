# Ammonia Monitoring System Setup Guide

## What You Need

### Hardware
- 2× ESP32 boards (one for each trash can)
- 2× MQ2 gas sensors
- Power supplies for each ESP32
- Computer with USB ports
- USB cables to program the ESP32s

### Software
1. Arduino IDE (version 2.0 or newer)
2. Python (version 3.8 or newer)

## Step-By-Step Setup

### 1. Install Software (Do This First)

On Windows:
- Download and install Arduino IDE from: https://arduino.cc/en/software
- Download and install Python from: https://python.org/downloads

On Linux (Ubuntu/Debian):
```bash
sudo apt update
sudo apt install python3 python3-pip
sudo apt install arduino
```

### 2. Set Up Arduino IDE

1. Open Arduino IDE
2. Go to File → Preferences
3. Add this URL to Additional Boards Manager URLs:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-master/package_esp32_index.json
   ```
4. Go to Tools → Board → Boards Manager
5. Search for "ESP32" and install "ESP32 by Espressif Systems"

### 3. Set Up Python Environment

Open a terminal/command prompt and run:
```bash
pip install flask flask-mqtt flask-socketio eventlet
```

### 4. Program the Sensor Nodes

1. Connect first ESP32 to your computer
2. Open Arduino IDE
3. Load the `IoT/Node.ino` file
4. Change these lines in the code:
   - Set `sensorID = 1` for first sensor
   - Set `gatewayAddress` to match your gateway's MAC (you'll get this later)
5. Select your board: Tools → Board → ESP32 Dev Module
6. Select the correct port: Tools → Port → (select your USB port)
7. Click Upload button
8. Repeat for second ESP32 but set `sensorID = 2`

### 5. Program the Gateway

1. Connect another ESP32 (this will be your gateway)
2. Open Arduino IDE
3. Load the `IoT/Gateway.ino` file
4. Change these lines:
   ```cpp
   const char* ssid = "YOUR_WIFI_NAME";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
5. Upload the code
6. Open Serial Monitor (Tools → Serial Monitor)
7. Copy the MAC address that appears - you'll need this for the sensor nodes

### 6. Start the Server

1. Open terminal/command prompt
2. Go to project folder
3. Run:
   ```bash
   python app.py
   ```
4. Open web browser
5. Go to: http://localhost:5000

## Testing the Setup

1. Power up both sensor nodes and the gateway
2. Check the web interface - you should see:
   - Two entries in the table (Trash Can A and B)
   - Numbers updating every 30 seconds
   - Total ammonia level at bottom

## Troubleshooting

If Nothing Shows Up:
1. Check all power connections
2. Make sure gateway shows "CONNECTED TO WIFI" in Serial Monitor
3. Make sure sensor nodes show "SENT READING SUCCESSFULLY"
4. Check your WiFi name and password are correct in gateway code

If Readings Look Wrong:
1. MQ2 sensors need 24-48 hours to stabilize
2. Keep sensors away from direct air flow
3. May need calibration (adjust the conversion formula in Node.ino)

## Need Help?

If something's not working:
1. Check all connections
2. Restart everything
3. Look for error messages in:
   - Arduino Serial Monitor
   - Python terminal
   - Web browser console (F12 to open)

## Maintenance

- Clean sensors every 3-6 months with compressed air
- Check and tighten connections monthly
- Download CSV data periodically for backup
