#include "neopixel.h"
#include <Arduino.h>
#include "config.h"

// NeoPixel object
Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// NeoPixel state variables
unsigned long lastNeoPixelUpdate = 0;
int neoPixelBrightness = 0;
bool neoPixelFadeDirection = true;  // true = fade in, false = fade out

extern bool dehumidifierRunning;
extern bool floatSwitchTriggered;

void updateNeoPixel() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastNeoPixelUpdate >= NEO_PIXEL_UPDATE_INTERVAL) {
    lastNeoPixelUpdate = currentMillis;
    
    if (floatSwitchTriggered) {
      // Flash bright red when tank is full (problem)
      if (neoPixelFadeDirection) {
        neoPixelBrightness += 25;  // Keep fast fade for warning
        if (neoPixelBrightness >= 255) {
          neoPixelBrightness = 255;
          neoPixelFadeDirection = false;
        }
      } else {
        neoPixelBrightness -= 25;  // Keep fast fade for warning
        if (neoPixelBrightness <= 0) {
          neoPixelBrightness = 0;
          neoPixelFadeDirection = true;
        }
      }
      pixel.setPixelColor(0, pixel.Color(neoPixelBrightness, 0, 0));  // Fading red
      pixel.show();
    } else if (dehumidifierRunning) {
      // Solid dim green when compressor is running
      pixel.setPixelColor(0, pixel.Color(0, 50, 0));  // Dim green
      pixel.show();
    } else {
      // Fade dim green when compressor is off
      if (neoPixelFadeDirection) {
        neoPixelBrightness += 2;  // Increased from 1 to 2 for faster fade
        if (neoPixelBrightness >= 50) {
          neoPixelBrightness = 50;
          neoPixelFadeDirection = false;
        }
      } else {
        neoPixelBrightness -= 2;  // Increased from 1 to 2 for faster fade
        if (neoPixelBrightness <= 0) {
          neoPixelBrightness = 0;
          neoPixelFadeDirection = true;
        }
      }
      pixel.setPixelColor(0, pixel.Color(0, neoPixelBrightness, 0));  // Fading dim green
      pixel.show();
    }
  }
}

