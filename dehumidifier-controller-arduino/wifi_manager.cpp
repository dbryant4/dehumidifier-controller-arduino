#include "wifi_manager.h"
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <EEPROM.h>
#include "config.h"
#include "display.h"  // For showProgressBar function

// WiFi credentials storage
String wifi_ssid = "";
String wifi_password = "";
bool wifiCredentialsValid = false;
bool isAPMode = false;

extern Adafruit_ST7789 tft;

void saveWiFiCredentials(String ssid, String password) {
  // Clear the credentials valid flag first
  EEPROM.writeByte(WIFI_CREDENTIALS_VALID_ADDR, 0);
  EEPROM.commit();
  
  // Save SSID (max 32 bytes)
  for (int i = 0; i < 32; i++) {
    if (i < ssid.length()) {
      EEPROM.writeByte(WIFI_SSID_ADDR + i, ssid.charAt(i));
    } else {
      EEPROM.writeByte(WIFI_SSID_ADDR + i, 0);
    }
  }
  
  // Save password (max 64 bytes)
  for (int i = 0; i < 64; i++) {
    if (i < password.length()) {
      EEPROM.writeByte(WIFI_PASSWORD_ADDR + i, password.charAt(i));
    } else {
      EEPROM.writeByte(WIFI_PASSWORD_ADDR + i, 0);
    }
  }
  
  // Mark credentials as valid
  EEPROM.writeByte(WIFI_CREDENTIALS_VALID_ADDR, 1);
  EEPROM.commit();
  
  Serial.println("WiFi credentials saved to EEPROM");
  Serial.print("SSID: ");
  Serial.println(ssid);
}

void loadWiFiCredentials() {
  // Check if credentials are valid
  byte valid = EEPROM.readByte(WIFI_CREDENTIALS_VALID_ADDR);
  if (valid != 1) {
    Serial.println("No valid WiFi credentials found in EEPROM");
    wifiCredentialsValid = false;
    return;
  }
  
  // Load SSID
  wifi_ssid = "";
  for (int i = 0; i < 32; i++) {
    byte c = EEPROM.readByte(WIFI_SSID_ADDR + i);
    if (c == 0) break;
    wifi_ssid += (char)c;
  }
  
  // Load password
  wifi_password = "";
  for (int i = 0; i < 64; i++) {
    byte c = EEPROM.readByte(WIFI_PASSWORD_ADDR + i);
    if (c == 0) break;
    wifi_password += (char)c;
  }
  
  if (wifi_ssid.length() > 0) {
    wifiCredentialsValid = true;
    Serial.print("Loaded WiFi credentials from EEPROM - SSID: ");
    Serial.println(wifi_ssid);
  } else {
    wifiCredentialsValid = false;
    Serial.println("Invalid WiFi credentials in EEPROM");
  }
}

bool connectToWiFi(String ssid, String password, int timeoutSeconds) {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  int maxAttempts = timeoutSeconds * 2;
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    // Update progress bar
    int progress = (attempts * 100) / maxAttempts;
    if (progress > 100) progress = 100;
    showProgressBar(progress, "Connecting...");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("WiFi connected! IP: ");
    Serial.println(WiFi.localIP());
    isAPMode = false;
    return true;
  } else {
    Serial.println("");
    Serial.println("WiFi connection failed!");
    return false;
  }
}

void startAPMode() {
  Serial.println("Starting Access Point mode...");
  isAPMode = true;
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("AP Password: ");
  Serial.println(AP_PASSWORD);
  
  // Update display with clear WiFi setup information
  tft.fillScreen(ST77XX_BLACK);
  
  // Title
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 5);
  tft.println("WiFi Setup");
  
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
  tft.println("Connect & portal");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 122);
  tft.println("will open auto");
}

int getWiFiSignalStrength() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  int rssi = WiFi.RSSI();
  // Convert RSSI to percentage (RSSI typically ranges from -100 to -30)
  // -30 is excellent signal, -100 is poor signal
  int percentage = map(rssi, -100, -30, 0, 100);
  return constrain(percentage, 0, 100);
}

