#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
SKETCH_NAME="dehumidifier-controller-arduino"
BOARD="esp32:esp32:adafruit_feather_esp32s2_tft"  # Updated to match working script
PORT="/dev/tty.usbserial-*"  # Adjust this based on your system
BUILD_DIR="build"
BIN_FILE="${SKETCH_NAME}.ino.bin"

echo -e "${YELLOW}Building ${SKETCH_NAME} for ESP32-S2...${NC}"

# Check if arduino-cli is installed
if ! command -v arduino-cli &> /dev/null; then
    echo -e "${RED}Error: arduino-cli is not installed${NC}"
    echo "Please install it first:"
    echo "curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
    exit 1
fi

# Create build directory if it doesn't exist
mkdir -p ${BUILD_DIR}

# Update core and libraries
echo -e "${YELLOW}Updating Arduino core and libraries...${NC}"
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib update-index

# Install libraries with proper commands
echo -e "${YELLOW}Installing required libraries...${NC}"
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit ST7735 and ST7789 Library"
arduino-cli config set library.enable_unsafe_install true
arduino-cli lib install "Adafruit Unified Sensor"
arduino-cli lib install --git-url https://github.com/adafruit/Adafruit_SHT4x.git
arduino-cli lib install --git-url https://github.com/adafruit/Adafruit_SHTC3.git
arduino-cli lib install "Adafruit NeoPixel"
arduino-cli lib install "PubSubClient"

# Compile the sketch
echo -e "${YELLOW}Compiling sketch...${NC}"
arduino-cli compile --fqbn ${BOARD} --output-dir ${BUILD_DIR} ${SKETCH_NAME}

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo -e "${GREEN}Compilation successful!${NC}"
    
    # Copy the binary to the current directory
    cp "${BUILD_DIR}/${BIN_FILE}" .
    echo -e "${GREEN}Binary file created: ${BIN_FILE}${NC}"
    echo -e "${YELLOW}You can now upload this file through the web interface${NC}"
    
    # Show file size
    FILESIZE=$(stat -f%z "${BIN_FILE}")
    echo -e "File size: ${FILESIZE} bytes"
else
    echo -e "${RED}Compilation failed!${NC}"
    exit 1
fi

# Optional: Upload directly via serial
read -p "Do you want to upload via serial port? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}Uploading via serial port...${NC}"
    arduino-cli upload -p ${PORT} --fqbn ${BOARD} --input-dir ${BUILD_DIR} ${SKETCH_NAME}
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Upload successful!${NC}"
    else
        echo -e "${RED}Upload failed!${NC}"
        exit 1
    fi
fi 