# Code Refactoring Notes

This codebase has been refactored into modular components for better organization and maintainability.

## Module Structure

### Core Modules:
- **config.h** - Pin definitions, EEPROM addresses, timing constants, version info
- **wifi_manager.h/cpp** - WiFi credential management, AP mode, connection handling
- **sensors.h/cpp** - Sensor initialization and reading (SHT4x/SHTC3)
- **display.h/cpp** - Display initialization and update functions
- **button.h/cpp** - Button handling logic
- **neopixel.h/cpp** - NeoPixel status indicator control
- **dehumidifier.h/cpp** - Dehumidifier control logic and float switch monitoring
- **web_server.h/cpp** - Web server handlers (to be created)
- **ota_manager.h/cpp** - OTA update handling (to be created)
- **mqtt_manager.h/cpp** - MQTT integration (to be created, conditional on ENABLE_HOME_ASSISTANT)

### Main File:
- **dehumidifier-controller-arduino.ino** - Main setup() and loop() functions, includes all modules

## Benefits of This Structure:
1. **Separation of Concerns** - Each module handles a specific functionality
2. **Easier Maintenance** - Changes to one module don't affect others
3. **Reusability** - Modules can be reused in other projects
4. **Testability** - Individual modules can be tested independently
5. **Readability** - Main file is cleaner and easier to understand

## Next Steps:
The web_server, ota_manager, and mqtt_manager modules still need to be extracted from the main .ino file to complete the refactoring.

