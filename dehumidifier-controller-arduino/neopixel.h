#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <Adafruit_NeoPixel.h>
#include "config.h"

// Global NeoPixel variables (extern declarations)
extern Adafruit_NeoPixel pixel;
extern unsigned long lastNeoPixelUpdate;
extern int neoPixelBrightness;
extern bool neoPixelFadeDirection;
extern bool dehumidifierRunning;
extern bool floatSwitchTriggered;

// Function declarations
void updateNeoPixel();

#endif // NEOPIXEL_H

