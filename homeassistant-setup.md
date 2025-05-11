# Home Assistant Integration Guide

This guide will walk you through setting up your dehumidifier controller with Home Assistant using MQTT auto-discovery.

## Prerequisites

1. A working Home Assistant instance
2. MQTT broker installed and configured in Home Assistant
3. The dehumidifier controller with Home Assistant integration enabled

## Step 1: Enable Home Assistant Integration

1. Open `dehumidifier-controller-arduino/dehumidifier-controller-arduino.ino` in the Arduino IDE
2. Find the configuration section and set:
   ```cpp
   #define ENABLE_HOME_ASSISTANT true
   ```
3. Update the MQTT settings with your broker information:
   ```cpp
   const char* mqtt_server = "YOUR_MQTT_BROKER";  // e.g., "192.168.1.100"
   const int mqtt_port = 1883;
   const char* mqtt_user = "YOUR_MQTT_USERNAME";  // Optional
   const char* mqtt_password = "YOUR_MQTT_PASSWORD";  // Optional
   ```
4. Upload the updated code to your device

## Step 2: Configure MQTT Broker in Home Assistant

1. In Home Assistant, go to **Settings** > **Devices & Services**
2. Click **Add Integration**
3. Search for "MQTT" and select it
4. Configure your MQTT broker:
   - Broker: Your MQTT broker address (e.g., "192.168.1.100")
   - Port: 1883 (default)
   - Username: Your MQTT username (if configured)
   - Password: Your MQTT password (if configured)
5. Click **Submit**

## Step 3: Verify Integration

1. After uploading the code, your device will automatically appear in Home Assistant
2. Go to **Settings** > **Devices & Services**
3. Look for "Dehumidifier Controller" under the MQTT integration
4. The device should show as "online" if everything is working correctly

## Available Controls and Sensors

The integration provides the following entities:

### Climate Entity
- **Name**: Dehumidifier Controller
- **Type**: Climate
- **Features**:
  - Current humidity
  - Target humidity
  - Current temperature
  - Operation mode (on/off)
  - Cooldown status

### Binary Sensors
- **Drain Status**: Indicates if the drain is full
- **Compressor Status**: Shows if the compressor is running
- **Cooldown Status**: Indicates if the compressor is in cooldown period

### Sensors
- **Current Humidity**: Real-time humidity reading
- **Current Temperature**: Real-time temperature reading
- **Target Humidity**: Current target humidity setting
- **WiFi Signal Strength**: Signal strength in dBm

## Creating Automations

Here are some example automations you can create:

### 1. Notify When Drain is Full
```yaml
automation:
  - alias: "Dehumidifier Drain Full Alert"
    trigger:
      platform: state
      entity_id: binary_sensor.dehumidifier_drain_status
      to: "on"
    action:
      - service: notify.mobile_app
        data:
          message: "Dehumidifier drain is full! Please empty the tank."
```

### 2. Turn Off When Away
```yaml
automation:
  - alias: "Turn Off Dehumidifier When Away"
    trigger:
      platform: state
      entity_id: person.your_name
      to: "not_home"
    action:
      - service: climate.set_hvac_mode
        target:
          entity_id: climate.dehumidifier_controller
        data:
          hvac_mode: "off"
```

### 3. Adjust Humidity Based on Time
```yaml
automation:
  - alias: "Set Night Humidity"
    trigger:
      platform: time
      at: "22:00:00"
    action:
      - service: climate.set_humidity
        target:
          entity_id: climate.dehumidifier_controller
        data:
          humidity: 45
```

## Troubleshooting

### Device Not Appearing
1. Verify MQTT broker is running and accessible
2. Check device's WiFi connection
3. Verify MQTT credentials are correct
4. Check Home Assistant logs for MQTT errors

### Device Offline
1. Check device's power supply
2. Verify WiFi connection
3. Check MQTT broker connection
4. Look for error messages in device's serial output

### Controls Not Working
1. Verify MQTT topics are correct
2. Check device's serial output for MQTT errors
3. Verify the device is receiving commands
4. Check Home Assistant logs for command errors

### Sensor Readings Not Updating
1. Check device's sensor connections
2. Verify MQTT state updates are being sent
3. Check Home Assistant logs for MQTT state errors
4. Verify the device is not in cooldown period

## Advanced Configuration

### Custom MQTT Topics
You can modify the MQTT topics in the code:
```cpp
const char* mqtt_topic_state = "homeassistant/climate/dehumidifier/state";
const char* mqtt_topic_command = "homeassistant/climate/dehumidifier/set";
const char* mqtt_topic_availability = "homeassistant/climate/dehumidifier/availability";
const char* mqtt_topic_config = "homeassistant/climate/dehumidifier/config";
```

### State Persistence
The device saves its state to EEPROM:
- Target humidity
- Last compressor off time
- Cooldown status

### Update Interval
The device publishes state updates every 5 seconds. You can modify this in the code:
```cpp
if (millis() - lastMqttPublish >= 5000) {
  publishMQTTState();
  lastMqttPublish = millis();
}
```

## Security Considerations

1. Use strong MQTT passwords
2. Consider using TLS for MQTT connections
3. Keep your Home Assistant instance updated
4. Use a dedicated MQTT user for the device
5. Regularly check device logs for unusual activity

## Support

If you encounter issues:
1. Check the device's serial output
2. Review Home Assistant logs
3. Verify MQTT broker logs
4. Check the device's WiFi connection
5. Verify all connections and power supply 