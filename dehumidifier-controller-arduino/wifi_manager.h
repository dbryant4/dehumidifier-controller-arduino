#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <EEPROM.h>
#include "config.h"

// WiFi credentials storage (extern declarations)
extern String wifi_ssid;
extern String wifi_password;
extern bool wifiCredentialsValid;
extern bool isAPMode;

// Function declarations
void saveWiFiCredentials(String ssid, String password);
void loadWiFiCredentials();
bool connectToWiFi(String ssid, String password, int timeoutSeconds = 20);
void startAPMode();
int getWiFiSignalStrength();

#endif // WIFI_MANAGER_H

