#ifndef CONFIG_H
#define CONFIG_H

// Version information
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 2
#define VERSION_BUILD __DATE__ " " __TIME__

// Configuration flags
#define ENABLE_HOME_ASSISTANT false  // Set to true to enable Home Assistant integration

// Pin definitions
#define TFT_CS        7
#define TFT_RST       40
#define TFT_DC        39
#define TFT_BACKLITE  45  // Backlight pin
#define TFT_I2C_POWER 21 // Power pin for TFT
#define BOOT_BUTTON 0  // Use BOOT button (GPIO 0)
#define DEHUMIDIFIER_RELAY 13
#define FLOAT_SWITCH 12

// EEPROM settings
#define EEPROM_SIZE 512
#define TARGET_HUMIDITY_ADDR 0
#define LAST_COMPRESSOR_OFF_ADDR 4  // 4 bytes for timestamp
#define COMPRESSOR_COOLDOWN_FLAG_ADDR 8  // 1 byte for cooldown flag
#define WIFI_SSID_ADDR 16  // WiFi SSID (max 32 bytes)
#define WIFI_PASSWORD_ADDR 48  // WiFi Password (max 64 bytes)
#define WIFI_CREDENTIALS_VALID_ADDR 112  // 1 byte flag to indicate if credentials are valid
#define FLOAT_SWITCH_PIN_ADDR 113  // 1 byte for float switch pin
#define COMPRESSOR_RELAY_PIN_ADDR 114  // 1 byte for compressor relay pin
#define PIN_CONFIG_VALID_ADDR 115  // 1 byte flag to indicate if pin config is valid
#define FLOAT_SWITCH_INVERTED_ADDR 116  // 1 byte flag to indicate if float switch is inverted

// Timing constants
#define SENSOR_READ_INTERVAL 2000  // Read sensors every 2 seconds
#define ANIMATION_UPDATE_INTERVAL 20  // Animation update interval in ms
#define DISPLAY_UPDATE_INTERVAL 5  // Display update interval in ms
#define STATUS_BAR_UPDATE_INTERVAL 1000  // Status bar update interval in ms
#define NEO_PIXEL_UPDATE_INTERVAL 10  // NeoPixel update interval in ms
#define WIFI_RECONNECT_INTERVAL 5000  // WiFi reconnect interval in ms
#define DEBOUNCE_TIME 50  // Button debounce time in milliseconds
#define LONG_PRESS_TIME 2000  // Long press threshold in milliseconds
#define COMPRESSOR_MIN_OFF_TIME 300000  // 5 minutes in milliseconds
#define UPDATE_TIMEOUT 300000  // 5 minutes timeout for updates

#if ENABLE_HOME_ASSISTANT
#define MQTT_RECONNECT_INTERVAL 5000  // MQTT reconnect interval in ms
#endif

// OTA settings
#define OTA_HOSTNAME "dehumidifier-controller"

// AP mode settings
#define AP_SSID "Dehumidifier-Setup"
#define AP_PASSWORD "setup12345"

#endif // CONFIG_H

