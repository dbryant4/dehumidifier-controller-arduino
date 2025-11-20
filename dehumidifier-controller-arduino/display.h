#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include "config.h"
#include "wifi_manager.h"

// Global display variables (extern declarations)
extern Adafruit_ST7789 tft;
extern unsigned long lastDisplayUpdate;
extern unsigned long lastStatusBarUpdate;
extern bool isAPMode;
extern bool dehumidifierRunning;
extern bool floatSwitchTriggered;
extern bool compressorInCooldown;
extern float currentHumidity;
extern float currentTemperature;
extern float targetHumidity;

// Function declarations
void initDisplay();
void updateDisplay();
void updateStatusBar();
void updateMainContent();
void showStartupAnimation();
void showStatusMessage(const String& message, bool clearScreen = false);
void showProgressBar(int percentage, const String& label = "");

#endif // DISPLAY_H

