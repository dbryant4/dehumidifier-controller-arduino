#include "sensors.h"
#include "config.h"

// Sensor objects
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_SHTC3 shtc3 = Adafruit_SHTC3();

// Sensor type tracking
bool usingSHT4x = false;
bool usingSHTC3 = false;

// Sensor readings
float currentHumidity = 0.0;
float currentTemperature = 0.0;

void initSensors() {
  // Initialize sensors
  if (sht4.begin()) {
    usingSHT4x = true;
    Serial.println("SHT4x sensor initialized successfully");
  } else if (shtc3.begin()) {
    usingSHTC3 = true;
    Serial.println("SHTC3 sensor initialized successfully");
  } else {
    Serial.println("WARNING: No supported sensor found! Device will continue without sensor readings.");
    // Don't hang - allow device to continue initialization
    // Sensor readings will remain at 0.0 until a sensor is connected
  }
}

void readSensors() {
  // Only read sensors if one is initialized
  if (!usingSHT4x && !usingSHTC3) {
    // No sensor available - keep default values (0.0)
    return;
  }
  
  sensors_event_t humidity, temp;
  
  if (usingSHT4x) {
    sht4.getEvent(&humidity, &temp);
    currentHumidity = humidity.relative_humidity;
    currentTemperature = temp.temperature;
  } else if (usingSHTC3) {
    shtc3.getEvent(&humidity, &temp);
    currentHumidity = humidity.relative_humidity;
    currentTemperature = temp.temperature;
  }
}

