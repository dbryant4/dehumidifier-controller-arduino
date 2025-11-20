#ifndef SENSORS_H
#define SENSORS_H

#include <Adafruit_SHT4x.h>
#include <Adafruit_SHTC3.h>

// Global sensor variables (extern declarations)
extern Adafruit_SHT4x sht4;
extern Adafruit_SHTC3 shtc3;
extern bool usingSHT4x;
extern bool usingSHTC3;
extern float currentHumidity;
extern float currentTemperature;

// Function declarations
void initSensors();
void readSensors();

#endif // SENSORS_H

