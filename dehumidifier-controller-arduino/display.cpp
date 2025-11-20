#include "display.h"
#include <WiFi.h>
#include <Arduino.h>
#include "wifi_manager.h"
#include "config.h"
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// Display object
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Display update timing
unsigned long lastDisplayUpdate = 0;
unsigned long lastStatusBarUpdate = 0;

extern bool isAPMode;
extern bool dehumidifierRunning;
extern bool floatSwitchTriggered;
extern bool compressorInCooldown;
extern float currentHumidity;
extern float currentTemperature;
extern float targetHumidity;

void initDisplay() {
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);
  
  tft.init(135, 240);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
  
  // Show startup animation
  showStartupAnimation();
}

void showStartupAnimation() {
  // Clear screen
  tft.fillScreen(ST77XX_BLACK);
  
  // Show title
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(20, 30);
  tft.println("Dehumidifier");
  tft.setCursor(50, 55);
  tft.println("Controller");
  
  // Animated loading bar
  int barWidth = 200;
  int barHeight = 8;
  int x = (240 - barWidth) / 2;
  int y = 100;
  
  // Draw border
  tft.drawRect(x, y, barWidth, barHeight, ST77XX_WHITE);
  
  // Animate progress bar
  for (int i = 0; i <= 100; i += 2) {
    int progressWidth = (barWidth * i) / 100;
    
    // Fill progress
    tft.fillRect(x + 1, y + 1, progressWidth - 2, barHeight - 2, ST77XX_GREEN);
    
    // Show percentage
    tft.fillRect(x, y + barHeight + 5, barWidth, 15, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(x + (barWidth / 2) - 15, y + barHeight + 10);
    tft.print(i);
    tft.print("%");
    
    delay(20);
  }
  
  // Brief pause
  delay(200);
  
  // Fade out effect
  for (int brightness = 255; brightness >= 0; brightness -= 10) {
    tft.fillScreen(ST77XX_BLACK);
    uint16_t color = tft.color565(brightness, brightness, brightness);
    tft.setTextColor(color);
    tft.setTextSize(2);
    tft.setCursor(20, 30);
    tft.println("Dehumidifier");
    tft.setCursor(50, 55);
    tft.println("Controller");
    delay(10);
  }
  
  // Clear screen for next step
  tft.fillScreen(ST77XX_BLACK);
}

void showStatusMessage(const String& message, bool clearScreen) {
  if (clearScreen) {
    tft.fillScreen(ST77XX_BLACK);
  }
  
  // Clear message area (middle of screen)
  tft.fillRect(0, 50, 240, 60, ST77XX_BLACK);
  
  // Show message
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  
  // Center the text (approximate)
  int textWidth = message.length() * 12; // Approximate width
  int x = (240 - textWidth) / 2;
  if (x < 0) x = 10;
  
  tft.setCursor(x, 70);
  tft.println(message);
}

void showProgressBar(int percentage, const String& label) {
  // Clear area
  tft.fillRect(0, 50, 240, 80, ST77XX_BLACK);
  
  // Show label if provided
  if (label.length() > 0) {
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 55);
    tft.println(label);
  }
  
  // Draw progress bar
  int barWidth = 220;
  int barHeight = 12;
  int x = (240 - barWidth) / 2;
  int y = 75;
  
  // Draw border
  tft.drawRect(x, y, barWidth, barHeight, ST77XX_WHITE);
  
  // Fill progress
  int progressWidth = (barWidth * percentage) / 100;
  if (progressWidth > 0) {
    tft.fillRect(x + 1, y + 1, progressWidth - 2, barHeight - 2, ST77XX_GREEN);
  }
  
  // Show percentage
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(x + (barWidth / 2) - 15, y + barHeight + 5);
  tft.print(percentage);
  tft.print("%");
}

void updateDisplay() {
  static bool firstRun = true;
  unsigned long currentMillis = millis();
  
  // Always allow first update, then respect interval
  if (!firstRun && (currentMillis - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL)) {
    return;
  }
  lastDisplayUpdate = currentMillis;
  firstRun = false;

  // If in AP mode, only update status bar, keep WiFi setup info visible
  if (isAPMode) {
    updateStatusBar();
    // Refresh the WiFi info display periodically to ensure it stays visible
    static unsigned long lastAPInfoRefresh = 0;
    if (currentMillis - lastAPInfoRefresh >= 10000) {  // Refresh every 10 seconds
      lastAPInfoRefresh = currentMillis;
      // Redraw WiFi setup info
      IPAddress IP = WiFi.softAPIP();
      tft.fillRect(0, 20, 240, 125, ST77XX_BLACK);
      
      // SSID
      tft.setTextSize(1);
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(10, 30);
      tft.print("SSID:");
      tft.setTextColor(ST77XX_CYAN);
      tft.setCursor(10, 42);
      tft.println(AP_SSID);
      
      // Password
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(10, 55);
      tft.print("Pass:");
      tft.setTextColor(ST77XX_CYAN);
      tft.setCursor(10, 67);
      tft.println(AP_PASSWORD);
      
      // IP Address
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(10, 80);
      tft.print("IP:");
      tft.setTextColor(ST77XX_GREEN);
      tft.setCursor(10, 92);
      tft.println(IP.toString());
      
      // Instructions
      tft.setTextColor(ST77XX_YELLOW);
      tft.setCursor(10, 110);
      tft.println("Connect & visit:");
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(10, 122);
      tft.println("http://" + IP.toString());
    }
    return;
  }

  // Only clear the screen on first run
  static bool firstContentRun = true;
  if (firstContentRun) {
    tft.fillScreen(ST77XX_BLACK);
    firstContentRun = false;
  }
  
  // Update status bar
  updateStatusBar();
  
  // Update main content
  updateMainContent();
}

void updateStatusBar() {
  static bool prevWifiConnected = false;
  static bool prevDehumidifierRunning = false;
  static bool prevFloatSwitchTriggered = false;
  static int prevRSSI = -999;
  static bool prevCompressorInCooldown = false;
  static bool firstRun = true;
  
  unsigned long currentMillis = millis();
  if (currentMillis - lastStatusBarUpdate < STATUS_BAR_UPDATE_INTERVAL && !firstRun) {
    return;
  }
  
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  bool needsUpdate = firstRun || (wifiConnected != prevWifiConnected || 
                     dehumidifierRunning != prevDehumidifierRunning || 
                     floatSwitchTriggered != prevFloatSwitchTriggered ||
                     compressorInCooldown != prevCompressorInCooldown ||
                     WiFi.RSSI() != prevRSSI);
  
  if (needsUpdate) {
    firstRun = false;
    lastStatusBarUpdate = currentMillis;
    tft.fillRect(0, 0, 240, 20, ST77XX_BLACK);
    
    // Calculate segment widths
    int segmentWidth = 240 / 3;
    
    // Draw tank status segment (left)
    uint16_t tankColor = floatSwitchTriggered ? ST77XX_RED : ST77XX_GREEN;
    tft.fillRect(0, 0, segmentWidth, 20, tankColor);
    tft.setTextColor(floatSwitchTriggered ? ST77XX_WHITE : ST77XX_BLACK);
    tft.setFont(&FreeMonoBold9pt7b);
    tft.setTextSize(1);
    tft.setCursor(5, 13);
    tft.print(floatSwitchTriggered ? "FULL" : "OK");
    
    // Draw relay status segment (middle)
    uint16_t relayColor;
    if (compressorInCooldown) {
      relayColor = 0xFFA500;  // Orange for cooldown
    } else {
      relayColor = dehumidifierRunning ? ST77XX_GREEN : 0x7BEF;
    }
    tft.fillRect(segmentWidth, 0, segmentWidth, 20, relayColor);
    tft.setTextColor(dehumidifierRunning ? ST77XX_BLACK : ST77XX_WHITE);
    tft.setCursor(segmentWidth + 5, 13);
    if (compressorInCooldown) {
      tft.print("COOL");
    } else {
      tft.print(dehumidifierRunning ? "ON" : "OFF");
    }
    
    // Draw WiFi status segment (right)
    uint16_t wifiColor;
    if (isAPMode) {
      wifiColor = ST77XX_YELLOW;  // Yellow for AP mode
    } else {
      wifiColor = wifiConnected ? ST77XX_BLACK : ST77XX_RED;
    }
    tft.fillRect(segmentWidth * 2, 0, segmentWidth, 20, wifiColor);
    tft.setTextColor(ST77XX_WHITE);
    
    if (isAPMode) {
      tft.setCursor(segmentWidth * 2 + 5, 13);
      tft.print("AP");
    } else if (wifiConnected) {
      // Draw signal strength bars
      int barWidth = 30;
      int barHeight = 4;
      int x = segmentWidth * 2 + 5;
      int y = 3;
      int numBars = 4;
      int signalStrength = getWiFiSignalStrength();
      
      // Calculate active bars based on signal strength
      int activeBars;
      uint16_t barColor;
      
      if (signalStrength >= 75) {
        activeBars = 4;
        barColor = ST77XX_GREEN;
      } else if (signalStrength >= 50) {
        activeBars = 3;
        barColor = 0x07E0;
      } else if (signalStrength >= 25) {
        activeBars = 2;
        barColor = 0xFFE0;
      } else {
        activeBars = 1;
        barColor = 0xFD20;
      }
      
      // Draw bars
      for (int i = 0; i < numBars; i++) {
        int barX = x + (i * (barWidth / numBars));
        int barY = y + (barHeight * (numBars - i - 1));
        uint16_t currentBarColor = (i < activeBars) ? barColor : 0x7BEF;
        tft.fillRect(barX, barY, (barWidth / numBars) - 1, barHeight, currentBarColor);
      }
      
      // Draw RSSI value
      int rssi = WiFi.RSSI();
      tft.fillRect(x + barWidth + 2, y, 40, 16, wifiColor);
      tft.setFont(&FreeMonoBold9pt7b);
      tft.setTextSize(1);
      tft.setCursor(x + barWidth + 2, 13);
      tft.print(rssi);
      
      prevRSSI = rssi;
    } else {
      tft.setCursor(segmentWidth * 2 + 5, 13);
      tft.print("OFF");
    }
    
    prevWifiConnected = wifiConnected;
    prevDehumidifierRunning = dehumidifierRunning;
    prevFloatSwitchTriggered = floatSwitchTriggered;
    prevCompressorInCooldown = compressorInCooldown;
  }
}

void updateMainContent() {
  static float prevHumidity = -999;
  static float prevTargetHumidity = -999;
  static float prevTemperature = -999;
  static bool firstRun = true;
  
  if (firstRun) {
    tft.fillRect(0, 45, 240, 90, ST77XX_BLACK);
    // Force initial display of all values
    prevHumidity = -999;
    prevTargetHumidity = -999;
    prevTemperature = -999;
    firstRun = false;
  }
  
  // Update humidity if changed (always draw on first run)
  float roundedCurrentHumidity = round(currentHumidity * 10.0) / 10.0;
  float roundedPrevHumidity = round(prevHumidity * 10.0) / 10.0;
  
  if (roundedCurrentHumidity != roundedPrevHumidity || prevHumidity == -999) {
    tft.fillRect(0, 15, 200, 55, ST77XX_BLACK);
    tft.setFont(&FreeSansBold18pt7b);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(15, 65);
    tft.print(currentHumidity, 1);
    tft.print("%");
    prevHumidity = currentHumidity;
  }
  
  // Update target humidity if changed (always draw on first run)
  if (targetHumidity != prevTargetHumidity || prevTargetHumidity == -999) {
    tft.fillRect(15, 75, 200, 20, ST77XX_BLACK);
    tft.setFont(&FreeMonoBold9pt7b);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(15, 85);
    tft.print("Target: ");
    tft.print(targetHumidity, 1);
    tft.print("%");
    prevTargetHumidity = targetHumidity;
  }
  
  // Draw separator line
  tft.drawLine(10, 92, 230, 92, ST77XX_WHITE);
  
  // Update temperature if changed (always draw on first run)
  if (currentTemperature != prevTemperature || prevTemperature == -999) {
    tft.fillRect(0, 97, 240, 20, ST77XX_BLACK);
    tft.setFont(&FreeMonoBold9pt7b);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(15, 112);
    tft.print(currentTemperature, 1);
    tft.print("C");
    
    float tempF = (currentTemperature * 9.0 / 5.0) + 32.0;
    tft.setCursor(80, 112);
    tft.print("(");
    tft.print(tempF, 1);
    tft.print("F)");
    prevTemperature = currentTemperature;
  }
  
  // Draw separator line
  tft.drawLine(10, 117, 230, 117, ST77XX_WHITE);
  
  // Always show IP address at the bottom (always draw on first run)
  static String lastIP = "";
  String currentIP;
  if (isAPMode) {
    currentIP = WiFi.softAPIP().toString();
  } else {
    currentIP = WiFi.localIP().toString();
  }
  
  if (currentIP != lastIP || lastIP == "") {
    tft.fillRect(0, 122, 240, 15, ST77XX_BLACK);
    tft.setFont(&FreeMonoBold9pt7b);
    tft.setTextSize(1);
    tft.setTextColor(isAPMode ? ST77XX_YELLOW : ST77XX_WHITE);
    tft.setCursor(15, 132);
    if (isAPMode) {
      tft.print("AP: ");
    } else {
      tft.print("IP: ");
    }
    tft.print(currentIP);
    lastIP = currentIP;
  }
}

