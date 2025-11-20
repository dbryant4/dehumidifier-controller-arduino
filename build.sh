#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SKETCH_NAME="dehumidifier-controller-arduino"
BOARD="esp32:esp32:adafruit_feather_esp32s2_tft"  # Updated to match working script
BUILD_DIR="build"
RELEASE_DIR="releases"

# Show help if requested
if [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
    echo "Usage: $0 [version]"
    echo ""
    echo "Build firmware for dehumidifier controller"
    echo ""
    echo "Arguments:"
    echo "  version    Optional version number (e.g., 1.0.0)"
    echo "             If not provided, reads from VERSION file"
    echo ""
    echo "Output:"
    echo "  - dehumidifier-controller-arduino.ino.bin (project root, for OTA)"
    echo "  - releases/dehumidifier-controller-arduino-v{VERSION}.bin (for GitHub releases)"
    echo ""
    echo "Examples:"
    echo "  $0              # Build using version from VERSION file"
    echo "  $0 1.0.0        # Build version 1.0.0"
    exit 0
fi

# Get version from VERSION file or command line argument
if [ -n "$1" ]; then
    VERSION="$1"
else
    if [ -f "VERSION" ]; then
        VERSION=$(cat VERSION | tr -d ' \n')
    else
        VERSION="unknown"
        echo -e "${RED}Warning: VERSION file not found and no version specified${NC}"
        echo "Usage: $0 [version] or create a VERSION file"
        exit 1
    fi
fi

# Create versioned binary filename
BIN_FILE="${SKETCH_NAME}.ino.bin"
VERSIONED_BIN_FILE="${SKETCH_NAME}-v${VERSION}.bin"

# Function to detect serial port
detect_port() {
    # Try to detect port using arduino-cli (preferred method)
    DETECTED_PORT=$(arduino-cli board list 2>/dev/null | grep -i "esp32" | grep -v "Unknown" | awk '{print $1}' | head -1)
    
    if [ -n "$DETECTED_PORT" ] && [ -e "$DETECTED_PORT" ]; then
        echo "$DETECTED_PORT"
        return 0
    fi
    
    # Fallback: try common port patterns (macOS uses /dev/cu.*, Linux uses /dev/ttyUSB*)
    for pattern in "/dev/cu.usbmodem*" "/dev/cu.usbserial*" "/dev/cu.SLAB*" "/dev/tty.usbserial*" "/dev/ttyUSB*"; do
        # Use find to avoid shell expansion issues
        PORT_CANDIDATE=$(find /dev -name "$(basename $pattern)" 2>/dev/null | head -1)
        if [ -n "$PORT_CANDIDATE" ] && [ -e "$PORT_CANDIDATE" ]; then
            echo "$PORT_CANDIDATE"
            return 0
        fi
    done
    
    return 1
}

echo -e "${YELLOW}Building ${SKETCH_NAME} v${VERSION} for ESP32-S2...${NC}"

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
    
    # Create releases directory if it doesn't exist
    mkdir -p ${RELEASE_DIR}
    
    # Copy the binary to current directory (for OTA uploads)
    cp "${BUILD_DIR}/${BIN_FILE}" .
    echo -e "${GREEN}Binary file created: ${BIN_FILE}${NC}"
    
    # Copy versioned binary to releases directory (for GitHub releases)
    cp "${BUILD_DIR}/${BIN_FILE}" "${RELEASE_DIR}/${VERSIONED_BIN_FILE}"
    echo -e "${GREEN}Versioned binary created: ${RELEASE_DIR}/${VERSIONED_BIN_FILE}${NC}"
    
    # Show file size
    FILESIZE=$(stat -f%z "${BIN_FILE}")
    echo -e "${BLUE}File size: ${FILESIZE} bytes${NC}"
    
    # Show release information
    echo ""
    echo -e "${BLUE}Release Information:${NC}"
    echo -e "  Version: ${GREEN}${VERSION}${NC}"
    echo -e "  Binary: ${GREEN}${RELEASE_DIR}/${VERSIONED_BIN_FILE}${NC}"
    echo -e "  Size: ${GREEN}${FILESIZE} bytes${NC}"
    echo ""
    echo -e "${YELLOW}You can now upload ${BIN_FILE} through the web interface${NC}"
    echo -e "${YELLOW}Or use ${RELEASE_DIR}/${VERSIONED_BIN_FILE} for GitHub releases${NC}"
else
    echo -e "${RED}Compilation failed!${NC}"
    exit 1
fi

# Optional: Upload directly via serial
read -p "Do you want to upload via serial port? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}Detecting serial port...${NC}"
    PORT=$(detect_port)
    
    if [ -z "$PORT" ]; then
        echo -e "${RED}Could not auto-detect serial port.${NC}"
        echo -e "${YELLOW}Available ports:${NC}"
        arduino-cli board list 2>/dev/null || echo "No boards detected"
        echo ""
        read -p "Enter serial port path manually (or press Enter to skip): " PORT
        if [ -z "$PORT" ]; then
            echo -e "${YELLOW}Upload skipped. You can upload the binary file manually.${NC}"
            exit 0
        fi
    else
        echo -e "${GREEN}Detected port: ${PORT}${NC}"
    fi
    
    echo -e "${YELLOW}Uploading via serial port...${NC}"
    arduino-cli upload -p "${PORT}" --fqbn ${BOARD} --input-dir ${BUILD_DIR} ${SKETCH_NAME}
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Upload successful!${NC}"
    else
        echo -e "${RED}Upload failed!${NC}"
        echo -e "${YELLOW}You can upload the binary file manually through the web interface${NC}"
        exit 1
    fi
fi 