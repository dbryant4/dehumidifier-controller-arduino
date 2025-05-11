#!/bin/bash
set -e

# Project and sketch names
SKETCH_DIR="dehumidifier-controller-arduino"
SKETCH_NAME="dehumidifier-controller-arduino.ino"
CONFIG_FILE="arduino-cli.yaml"
FQBN="esp32:esp32:adafruit_feather_esp32s2_tft"

# Ensure we're in the correct directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# 1. Install Arduino CLI if not present
if ! command -v arduino-cli &> /dev/null; then
  echo "Installing Arduino CLI..."
  brew install arduino-cli
fi

# 2. Update and install ESP32 platform
echo "Updating core index..."
arduino-cli core update-index
echo "Installing ESP32 platform..."
arduino-cli core install esp32:esp32

# 3. Install required libraries
echo "Installing required libraries..."
arduino-cli lib install "Adafruit GFX Library" "Adafruit ST7735 and ST7789 Library"

# Enable git URL support
echo "Enabling git URL support..."
arduino-cli config set library.enable_unsafe_install true

# Install Adafruit Sensor library (required for SHT4x)
echo "Installing Adafruit Sensor library..."
arduino-cli lib install "Adafruit Unified Sensor"

# SHT4x from GitHub
arduino-cli lib install --git-url https://github.com/adafruit/Adafruit_SHT4x.git

# Install SHTC3 library
echo "Installing SHTC3 library..."
arduino-cli lib install --git-url https://github.com/adafruit/Adafruit_SHTC3.git

# Install PubSubClient library for MQTT
echo "Installing PubSubClient library..."
arduino-cli lib install "PubSubClient"

# 4. Detect or use specified port
if [ -n "$1" ]; then
  PORT="$1"
  echo "Using user-specified port: $PORT"
else
  # Look specifically for ESP32-S2 devices
  PORT=$(arduino-cli board list | grep -i "esp32s2" | awk '{print $1}' | head -n 1)
  if [ -z "$PORT" ]; then
    echo "No ESP32-S2 device found. Please connect your ESP32-S2 and rerun this script."
    exit 1
  fi
  echo "Auto-detected ESP32-S2 port: $PORT"
fi

# 5. Compile the sketch
cd "$SKETCH_DIR"
echo "Compiling sketch..."
arduino-cli compile --fqbn "$FQBN"

# 6. Upload the sketch
echo "Uploading sketch..."
arduino-cli upload -p "$PORT" --fqbn "$FQBN"

# 7. Optionally, open serial monitor
# Automatically attach to serial monitor after upload
echo "Attaching to serial monitor (press Ctrl+C to exit)..."
arduino-cli monitor -p "$PORT" --config 115200 