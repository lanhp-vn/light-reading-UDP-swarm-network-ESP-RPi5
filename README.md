# UDP Communication Multi-ESP8266s with Raspberry Pi 5

## Project Overview

This project implements a distributed IoT system where multiple ESP8266 devices communicate with each other and a Raspberry Pi 5 using UDP protocol. The system features a master-slave architecture where ESP8266 devices compete for master status based on light sensor readings, with visual feedback through LED indicators.

## Demo Video

Demo Video: [UDP Communication Demo](https://youtu.be/VHKbJbl7Ohc)

## System Architecture

### Components
- **Multiple ESP8266 devices** - Each equipped with a photoresistor sensor and LED indicators
- **Raspberry Pi 5** - Central controller with GPIO buttons, RGB LEDs, and white LED
- **WiFi Network** - Enables UDP communication between all devices

### Communication Protocol
- **UDP Broadcast** - All devices communicate on port 4210
- **Message Delimiters**:
  - ESP-to-ESP: `~~~` (start) and `---` (end)
  - RPi-to-ESP: `+++` (start) and `***` (end)

## Features

### ESP8266 Devices
- **Automatic Swarm ID Assignment** - Based on IP address last digit
- **Light Sensor Integration** - Photoresistor readings determine LED flash rates
- **Master-Slave Competition** - Device with highest light reading becomes master
- **Dual LED Indicators**:
  - Indicator LED: Flashes based on sensor reading
  - Master LED: Only active when device is master
- **UDP Broadcasting** - Sends sensor data every 200ms if no message received
- **Reset Capability** - Responds to RPi reset commands

### Raspberry Pi 5
- **Button Control** - Physical button triggers system reset
- **RGB LED Management** - Assigns unique LED to each Swarm ID
- **Data Logging** - Records sensor readings to `sensor_readings.txt`
- **Visual Feedback** - White LED indicates reset status
- **Linear LED Mapping** - Maps sensor readings to LED flash intervals

## Hardware Requirements

### ESP8266 Setup
- ESP8266 development board
- Photoresistor (light sensor)
- 2 LEDs (indicator and master status)
- Appropriate resistors and wiring

### Raspberry Pi 5 Setup
- Raspberry Pi 5
- Push button (GPIO 26)
- White LED (GPIO 16)
- 3 RGB LEDs (GPIO 5, 6, 13)
- Appropriate resistors and wiring

## Software Requirements

### ESP8266 Dependencies
- ESP8266WiFi library
- WiFiUdp library

### Raspberry Pi 5 Dependencies
- Python 3.x
- gpiod library
- socket library
- threading library

## Installation and Setup

### 1. ESP8266 Configuration

1. **Install Arduino IDE** and ESP8266 board support
2. **Open** `ESP_code/ESP_code.ino` in Arduino IDE
3. **Update WiFi credentials**:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
4. **Upload code** to ESP8266 devices

### 2. Raspberry Pi 5 Configuration

1. **Install required packages**:
   ```bash
   sudo apt update
   sudo apt install python3-gpiod
   ```

2. **Run the Python script**:
   ```bash
   python3 RPi_pycode.py
   ```

### 3. Hardware Connections

#### ESP8266 Wiring
- **Photoresistor**: Connect to A0 pin
- **Indicator LED**: Connect to GPIO 2
- **Master LED**: Connect to GPIO 16
- **Power**: 3.3V and GND

#### Raspberry Pi 5 Wiring
- **Button**: GPIO 26 to GND (with pull-up resistor)
- **White LED**: GPIO 16 to GND (with current-limiting resistor)
- **RGB LEDs**: GPIO 5, 6, 13 to GND (with current-limiting resistors)

## Usage

### Starting the System

1. **Power on all ESP8266 devices** - They will automatically connect to WiFi
2. **Run the Raspberry Pi script** - It will start listening for UDP messages
3. **Observe LED behavior**:
   - ESP8266 indicator LEDs flash based on light intensity
   - Master LED only active on device with highest reading
   - RPi RGB LEDs flash for each unique Swarm ID

### System Reset

- **Press the button** on Raspberry Pi 5 to reset the entire system
- **All devices** will clear their states and restart competition
- **White LED** on RPi will flash for 3 seconds during reset

### Data Logging

- Sensor readings are automatically logged to `sensor_readings.txt`
- Format: `Swarm ID X: Y` where X is device ID and Y is analog reading

## Technical Details

### LED Flash Rate Calculation

The system uses linear interpolation to map sensor readings to LED flash intervals:

```
Interval = slope × reading + intercept
```

Where:
- **slope** = (Z_interval - Y_interval) / (Z_threshold - Y_threshold)
- **intercept** = Y_interval - slope × Y_threshold
- **Y_threshold** = 24, **Y_interval** = 2010ms
- **Z_threshold** = 1024, **Z_interval** = 10ms

### Master Selection Algorithm

1. Each device broadcasts its sensor reading
2. Devices compare their reading with received readings
3. Device with highest reading becomes master
4. Master device sends data to Raspberry Pi

### Communication Timing

- **Silent time**: 200ms (if no message received, device broadcasts)
- **Reset hold time**: 3000ms (3 seconds)
- **UDP port**: 4210
