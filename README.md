# Dehumidifier Controller

This project uses an Adafruit ESP32-S2 TFT Feather and either a Sensirion SHT40 or SHTC3 temperature/humidity sensor to control a dehumidifier with a broken control board. The system provides a user interface through a TFT display and web interface, allowing remote monitoring and control.

> **📋 Documentation**: For detailed requirements and specifications, see the [Product Requirements Document](.cursor/rules/prd.mdc). This README and the PRD are kept synchronized - both must be updated when making code changes.

## Hardware Requirements

- Adafruit ESP32-S2 TFT Feather (https://www.adafruit.com/product/5300)
- Temperature & Humidity Sensor (choose one):
  - Adafruit Sensirion SHT40 Temperature & Humidity Sensor (https://www.adafruit.com/product/4885)
  - Adafruit Sensirion SHTC3 Temperature & Humidity Sensor (https://www.adafruit.com/product/4636)
- Float switch for condensate drain monitoring
- Relay module for controlling the dehumidifier
- **BOOT button (built-in, GPIO 0) for local control**
- Power supply for the ESP32-S2
- Jumper wires

## Pin Connections

- TFT Display (built into ESP32-S2 TFT Feather)
  - CS: Pin 7
  - RST: Pin 40
  - DC: Pin 39
  - Backlight: Pin 45
  - I2C Power: Pin 21

- Local Control
  - BOOT Button: GPIO 0 (onboard)

- Dehumidifier Control
  - Relay Control: Pin 13
  - Float Switch: Pin 12

- Temperature/Humidity Sensor (STEMMA QT / Qwiic)
  - Connect to the STEMMA QT connector on the ESP32-S2 TFT Feather
  - Supports both SHT40 and SHTC3 sensors
  - The system will automatically detect and use whichever sensor is connected

## Connection Diagram

```
                    +------------------+
                    |   ESP32-S2 TFT   |
                    |     Feather      |
                    |                  |
                    |  +------------+  |
                    |  |   TFT      |  |
                    |  |  Display   |  |
                    |  +------------+  |
                    |                  |
+----------------+  |  +------------+  |
|                |  |  |   SHT40    |  |
|  BOOT Button   |--+--|  Sensor    |  |
| (GPIO 0, Onbd) |     +------------+  |
+----------------+                     |
                                       |
+----------------+                     |  +----------------+
|                |                     |  |                |
| Float Switch   |---------------------+--| Relay Control  |
| (Pin 12)       |                     |  | (Pin 13)       |
+----------------+                     |  +----------------+
                                       |
                                       |  +----------------+
                                       |  |                |
                                       +--| Dehumidifier   |
                                          | Power Control  |
                                          +----------------+
```

## Software Setup

1. Install the Arduino IDE
2. Install the following libraries through the Arduino Library Manager:
   - Adafruit GFX Library
   - Adafruit ST7789
   - Adafruit Unified Sensor
   - WiFi
   - WebServer
   - ArduinoOTA
   - PubSubClient (only if using Home Assistant integration)

3. The setup script will automatically install:
   - Adafruit SHT4x Library (from GitHub)
   - Adafruit SHTC3 Library (from GitHub)
   - PubSubClient Library (only if using Home Assistant integration)

4. Configure WiFi:
   - Open `dehumidifier-controller-arduino/dehumidifier-controller-arduino.ino`
   - Update the WiFi credentials:
     ```cpp
     const char* ssid = "YOUR_WIFI_SSID";
     const char* password = "YOUR_WIFI_PASSWORD";
     ```

5. (Optional) Configure Home Assistant Integration:
   - Set `ENABLE_HOME_ASSISTANT` to `true` in the code
   - Update the MQTT settings:
     ```cpp
     const char* mqtt_server = "YOUR_MQTT_BROKER";  // e.g., "192.168.1.100"
     const int mqtt_port = 1883;
     const char* mqtt_user = "YOUR_MQTT_USERNAME";  // Optional
     const char* mqtt_password = "YOUR_MQTT_PASSWORD";  // Optional
     ```

6. Upload the code to your ESP32-S2

## Optional: Home Assistant Integration

The device can optionally integrate with Home Assistant using MQTT auto-discovery. To enable this feature:

1. Set `ENABLE_HOME_ASSISTANT` to `true` in the code
2. Configure your MQTT broker settings
3. The device will automatically appear in Home Assistant as a climate entity

### Features in Home Assistant
- Real-time humidity and temperature monitoring
- Target humidity control
- Device status monitoring
- Cooldown period indication
- Drain status alerts
- Automatic reconnection handling
- State persistence across reboots

### MQTT Topics
- State: `homeassistant/climate/dehumidifier/state`
- Command: `homeassistant/climate/dehumidifier/set`
- Availability: `homeassistant/climate/dehumidifier/availability`
- Config: `homeassistant/climate/dehumidifier/config`

### Home Assistant Setup
1. Ensure you have the MQTT integration installed in Home Assistant
2. Configure your MQTT broker settings in the device
3. The device will automatically appear in Home Assistant
4. You can control the device through the Home Assistant interface

## Quick Setup Script

To automate the installation, compilation, and upload process, you can use the provided `setup_and_upload.sh` script. This script will:
- Install Arduino CLI (if not already installed)
- Install the ESP32 platform and required libraries
- Compile the code
- Upload it to your connected ESP32-S2 device

### Usage

1. **Put your ESP32-S2 TFT Feather into bootloader mode:**
   - Hold down the **BOOT** button (sometimes labeled as "0" or "BOOT") on the board.
   - While holding BOOT, press and release the **RESET** button.
   - Continue holding BOOT for a second, then release it. The board is now in bootloader mode and ready for firmware upload.
   - (You may see a new USB device appear, or a new serial port in `arduino-cli board list`.)

2. Make the script executable:
   ```bash
   chmod +x setup_and_upload.sh
   ```
3. Run the script from the project root:
   ```bash
   ./setup_and_upload.sh
   ```
4. Follow any prompts. The script will auto-detect your serial port and upload the code.

**Note:**
- Make sure your ESP32-S2 is connected via USB before running the script.
- If you want to monitor serial output after upload, follow the instructions printed at the end of the script.

## Features

- Real-time temperature and humidity display
- **Adjustable target humidity (30-70%) using the BOOT button (cycles in 5% steps)**
- Local control via the BOOT button (no external buttons required)
- Web interface for remote monitoring and control
- Automatic dehumidifier control based on humidity levels
- Float switch monitoring with warning display
- Animated warnings for drain full condition
- JSON API endpoint for data access
- Over-the-Air (OTA) firmware updates
- Status bar with tank level, relay status, and WiFi signal strength
- Smooth animations and high refresh rate display (200Hz)
- Clear visual separation between humidity and temperature readings
- WiFi signal strength indicator with 4-level bar display and RSSI value
- IP address display on long-press of BOOT button (3-second display)

## Display Features

- Compact status bar showing:
  - Tank level status (OK/FULL)
  - Relay status (ON/OFF/COOL)
  - WiFi signal strength bars (4 bars, color-coded)
    - 4 bars: Excellent signal (75%+, Green)
    - 3 bars: Good signal (50-74%, Yellow-green)
    - 2 bars: Fair signal (25-49%, Yellow)
    - 1 bar: Poor signal (<25%, Orange)
    - Raw RSSI value displayed next to bars
- Large, easy-to-read humidity display
- Target humidity setting
- Temperature in both Celsius and Fahrenheit
- Visual feedback for all status changes
- IP address display on long-press of BOOT button
- High refresh rate display (200Hz) for smooth updates
- Clear separation between humidity and temperature readings

## Web Interface

Once connected to WiFi, the device creates a web server that can be accessed at:
- Main interface: `http://<device-ip>/`
- JSON data: `http://<device-ip>/data`
- Firmware update: `http://<device-ip>/update`

The web interface allows you to:
- View current temperature and humidity
- Monitor dehumidifier status
- Check drain status
- Set target humidity (30-70% in 5% steps)
- Update firmware over-the-air

### OTA Updates

The device supports Over-the-Air (OTA) firmware updates through the web interface. There are two ways to perform updates:

#### Using the Build Script (Recommended)

1. Make sure you have Arduino CLI installed:
   ```bash
   brew install arduino-cli  # macOS
   # or
   apt install arduino-cli   # Linux
   ```

2. Run the build script:
   ```bash
   ./build.sh
   ```

   The script will:
   - Install required Arduino core and libraries
   - Compile the sketch
   - Generate a binary file in the current directory
   - Display the binary file size
   - Optionally upload via serial port

3. Once the binary is generated, visit `http://<device-ip>/update`
4. Select the generated `.bin` file
5. Click Upload and wait for the process to complete

#### Manual OTA Update

1. Compile your sketch in the Arduino IDE
2. Go to Sketch > Export Compiled Binary
3. Visit `http://<device-ip>/update`
4. Select the .bin file from your Arduino sketch folder
5. Click Upload and wait for the process to complete

The device will show update progress on both the web interface and the display. The update process includes:
- Progress bar display
- Automatic restart after successful update
- 5-minute timeout protection
- Error handling and display

**Note:** The build script is the recommended method as it ensures consistent compilation and handles all dependencies automatically.

## Safety Features

- Automatic shutdown when drain is full
- Visual and web-based warnings
- Hysteresis control to prevent rapid cycling
- Input validation for humidity settings
- Update timeout protection (5 minutes)
- Automatic restart after successful update
- Compressor cooldown protection (5 minutes)
- EEPROM state persistence

## Troubleshooting

1. If the display doesn't initialize:
   - Check the TFT connections
   - Verify the correct pins are defined
   - Ensure the backlight and I2C power pins are properly set

2. If the sensor isn't detected:
   - Check the STEMMA QT connection
   - Verify the sensor is properly powered
   - The system supports both SHT40 and SHTC3 sensors - make sure one is properly connected

3. If WiFi connection fails:
   - Verify the credentials
   - Check your network settings
   - Ensure the ESP32-S2 is within range
   - Check the WiFi signal strength indicator

4. If the dehumidifier doesn't respond:
   - Check the relay connections
   - Verify the float switch is properly connected
   - Check the power supply to the relay
   - Verify the compressor cooldown period has elapsed

5. If OTA update fails:
   - Ensure stable WiFi connection
   - Check that the .bin file is valid
   - Verify sufficient flash space
   - Wait for the 5-minute timeout to expire before trying again
   - Check the update progress on both web interface and display 