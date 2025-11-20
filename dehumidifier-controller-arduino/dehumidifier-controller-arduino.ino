#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <EEPROM.h>

#if ENABLE_HOME_ASSISTANT
#include <PubSubClient.h>  // Only include MQTT library if Home Assistant is enabled
#endif

// Include modular components
#include "config.h"
#include "wifi_manager.h"
#include "sensors.h"
#include "display.h"
#include "button.h"
#include "neopixel.h"
#include "dehumidifier.h"
#include "web_templates.h"

// OTA settings
const char* hostname = OTA_HOSTNAME;

#if ENABLE_HOME_ASSISTANT
// MQTT settings
const char* mqtt_server = "";  // Change this to your MQTT broker address
const int mqtt_port = 1883;
const char* mqtt_user = "";  // Optional
const char* mqtt_password = "";  // Optional
const char* mqtt_client_id = "dehumidifier-controller";

// MQTT topics
const char* mqtt_topic_state = "homeassistant/climate/dehumidifier/state";
const char* mqtt_topic_command = "homeassistant/climate/dehumidifier/set";
const char* mqtt_topic_availability = "homeassistant/climate/dehumidifier/availability";
const char* mqtt_topic_config = "homeassistant/climate/dehumidifier/config";

// MQTT client
WiFiClient espClient;
PubSubClient mqtt(espClient);

// Add these global variables at the top with other globals
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;  // Try to reconnect every 5 seconds
bool mqttConnected = false;
#endif

// Global variables (shared across modules)
float targetHumidity = 50.0;  // Default target humidity
unsigned long lastSensorRead = 0;
unsigned long lastAnimationUpdate = 0;
unsigned long lastWiFiReconnectAttempt = 0;
bool isUpdating = false;
bool updateComplete = false;  // Flag to indicate update completed successfully
unsigned long updateStartTime = 0;

// Pin configuration (configurable via web UI)
uint8_t floatSwitchPin = FLOAT_SWITCH;  // Default pin
uint8_t compressorRelayPin = DEHUMIDIFIER_RELAY;  // Default pin
bool floatSwitchInverted = false;  // Whether float switch logic is inverted

// Web server
WebServer server(80);

// DNS server for captive portal
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Extern declarations for variables in modules
extern bool isAPMode;
extern bool wifiCredentialsValid;
extern String wifi_ssid;
extern String wifi_password;
extern float currentHumidity;
extern float currentTemperature;
extern bool dehumidifierRunning;
extern bool floatSwitchTriggered;
extern bool compressorInCooldown;
extern Adafruit_ST7789 tft;
extern Adafruit_NeoPixel pixel;


// Helper function for version string
String getVersionString() {
  return String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + "." + String(VERSION_PATCH);
}

void setupOTA() {
  ArduinoOTA.setHostname(hostname);
  
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("Start updating " + type);
    Serial.println("Current version: " + getVersionString());
    
    // Clear display and show update message
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.println("Updating...");
    tft.setCursor(10, 70);
    tft.println("Please wait");
    tft.setCursor(10, 90);
    tft.println("v" + getVersionString());
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    // Show progress bar
    int barWidth = 200;
    int barHeight = 20;
    int x = (240 - barWidth) / 2;
    int y = 100;
    
    // Clear previous progress
    tft.fillRect(x, y, barWidth, barHeight, ST77XX_BLACK);
    
    // Draw progress bar border
    tft.drawRect(x, y, barWidth, barHeight, ST77XX_WHITE);
    
    // Draw progress
    int progressWidth = (progress * barWidth) / total;
    tft.fillRect(x + 1, y + 1, progressWidth - 2, barHeight - 2, ST77XX_GREEN);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nUpdate complete!");
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_GREEN);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.println("Update");
    tft.setCursor(10, 70);
    tft.println("Complete!");
    delay(2000);
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    
    // Show error on display
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.println("Update");
    tft.setCursor(10, 70);
    tft.println("Failed!");
    delay(2000);
  });
  
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

#if ENABLE_HOME_ASSISTANT
void setupMQTT() {
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(mqttCallback);
  
  // Publish Home Assistant discovery config
  String config = "{\"name\":\"Dehumidifier Controller\","
                 "\"unique_id\":\"dehumidifier_controller\","
                 "\"device_class\":\"humidity\","
                 "\"state_topic\":\"" + String(mqtt_topic_state) + "\","
                 "\"command_topic\":\"" + String(mqtt_topic_command) + "\","
                 "\"availability_topic\":\"" + String(mqtt_topic_availability) + "\","
                 "\"min_humidity\":30,"
                 "\"max_humidity\":70,"
                 "\"step\":5,"
                 "\"modes\":[\"off\",\"on\"],"
                 "\"current_temperature_topic\":\"" + String(mqtt_topic_state) + "\","
                 "\"current_temperature_template\":\"{{ value_json.temperature }}\","
                 "\"current_humidity_topic\":\"" + String(mqtt_topic_state) + "\","
                 "\"current_humidity_template\":\"{{ value_json.humidity }}\","
                 "\"target_humidity_topic\":\"" + String(mqtt_topic_state) + "\","
                 "\"target_humidity_template\":\"{{ value_json.target_humidity }}\","
                 "\"mode_state_topic\":\"" + String(mqtt_topic_state) + "\","
                 "\"mode_state_template\":\"{{ value_json.mode }}\"}";
  
  mqtt.publish(mqtt_topic_config, config.c_str(), true);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  if (String(topic) == mqtt_topic_command) {
    // Parse JSON command
    if (message.indexOf("\"target_humidity\"") >= 0) {
      int start = message.indexOf(":") + 1;
      int end = message.indexOf("}");
      String value = message.substring(start, end);
      float newHumidity = value.toFloat();
      if (newHumidity >= 30 && newHumidity <= 70 && fmod(newHumidity, 5) == 0) {
        targetHumidity = newHumidity;
        EEPROM.writeFloat(TARGET_HUMIDITY_ADDR, targetHumidity);
        EEPROM.commit();
      }
    }
  }
}

void reconnectMQTT() {
  if (!mqtt.connected()) {
    if (mqtt.connect(mqtt_client_id, mqtt_user, mqtt_password, mqtt_topic_availability, 0, true, "offline")) {
      mqtt.publish(mqtt_topic_availability, "online", true);
      mqtt.subscribe(mqtt_topic_command);
      mqttConnected = true;
    }
  }
}

void publishMQTTState() {
  if (mqtt.connected()) {
    String state = "{\"mode\":\"" + String(dehumidifierRunning ? "on" : "off") + "\","
                   "\"humidity\":" + String(currentHumidity) + ","
                   "\"temperature\":" + String(currentTemperature) + ","
                   "\"target_humidity\":" + String(targetHumidity) + ","
                   "\"cooldown\":" + String(compressorInCooldown ? "true" : "false") + ","
                   "\"drain_full\":" + String(floatSwitchTriggered ? "true" : "false") + "}";
    
    mqtt.publish(mqtt_topic_state, state.c_str(), true);
  }
}
#endif

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Dehumidifier Controller Starting ===");
  Serial.println("Version: " + getVersionString());
  Serial.println("Build: " + String(VERSION_BUILD));
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
  // Load saved target humidity
  float savedHumidity = EEPROM.readFloat(TARGET_HUMIDITY_ADDR);
  Serial.print("Raw saved humidity value: ");
  Serial.println(savedHumidity);
  
  // Validate the saved humidity value
  if (!isnan(savedHumidity) && savedHumidity >= 30.0 && savedHumidity <= 70.0) {
    targetHumidity = savedHumidity;
    Serial.print("Loaded saved target humidity: ");
    Serial.println(targetHumidity);
  } else {
    Serial.println("No valid saved humidity found, using default");
    // Save default value to EEPROM
    EEPROM.writeFloat(TARGET_HUMIDITY_ADDR, targetHumidity);
    EEPROM.commit();
  }
  
  // Load saved compressor state
  extern unsigned long lastCompressorOff;
  lastCompressorOff = EEPROM.readULong(LAST_COMPRESSOR_OFF_ADDR);
  extern bool compressorInCooldown;
  compressorInCooldown = EEPROM.readBool(COMPRESSOR_COOLDOWN_FLAG_ADDR);
  
  // If we're in cooldown, check if we should still be
  if (compressorInCooldown) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastCompressorOff >= COMPRESSOR_MIN_OFF_TIME) {
      compressorInCooldown = false;
      EEPROM.writeBool(COMPRESSOR_COOLDOWN_FLAG_ADDR, false);
      EEPROM.commit();
    }
  }
  
  // Initialize NeoPixel
  extern Adafruit_NeoPixel pixel;
  pixel.begin();
  pixel.setBrightness(50);
  pixel.show();
  
  // Initialize display (includes startup animation)
  initDisplay();
  
  // Show initialization status
  showStatusMessage("Initializing...", true);
  delay(500);
  
  // Initialize button and outputs (using configured pins)
  showStatusMessage("Setting up pins...", true);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  pinMode(compressorRelayPin, OUTPUT);
  pinMode(floatSwitchPin, INPUT_PULLUP);
  delay(300);
  
  // Load pin configuration from EEPROM
  loadPinConfiguration();
  
  // Initialize sensors
  showStatusMessage("Initializing sensors...", true);
  initSensors();
  delay(500);
  
  // Check if sensors initialized successfully (variables declared in sensors.h)
  if (!usingSHT4x && !usingSHTC3) {
    showStatusMessage("No sensor found!", true);
    delay(2000);
    showStatusMessage("Continuing anyway...", true);
    delay(1000);
  }
  
  // Load WiFi credentials from EEPROM
  showStatusMessage("Loading WiFi config...", true);
  loadWiFiCredentials();
  delay(300);
  
  // Try to connect to WiFi
  bool connected = false;
  if (wifiCredentialsValid) {
    showStatusMessage("Connecting to WiFi...", true);
    showProgressBar(0, "Connecting");
    
    // Show progress during connection attempt
    connected = connectToWiFi(wifi_ssid, wifi_password);
    
    if (connected) {
      showProgressBar(100, "Connected!");
      delay(500);
      showStatusMessage("WiFi Connected!", true);
      delay(1000);
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
    } else {
      showProgressBar(0, "Connection failed");
      delay(500);
    }
  } else {
    showStatusMessage("No WiFi config found", true);
    delay(1000);
  }
  
  if (!connected) {
    // If all connection attempts failed, start AP mode
    Serial.println("All WiFi connection attempts failed. Starting AP mode...");
    showStatusMessage("Starting AP mode...", true);
    delay(500);
    startAPMode();
    // Start DNS server for captive portal
    IPAddress apIP = WiFi.softAPIP();
    dnsServer.start(DNS_PORT, "*", apIP);
    Serial.println("DNS server started for captive portal");
  } else {
    // Setup OTA only if connected to WiFi
    showStatusMessage("Setting up OTA...", false);
    delay(300);
  setupOTA();
  }
  
  #if ENABLE_HOME_ASSISTANT
  // Setup MQTT (only if connected to WiFi, not in AP mode)
  if (!isAPMode && WiFi.status() == WL_CONNECTED) {
    showStatusMessage("Setting up MQTT...", false);
    delay(300);
  setupMQTT();
  }
  #endif
  
  // Setup web server routes
  showStatusMessage("Starting web server...", false);
  delay(300);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/json", HTTP_GET, handleData);
  server.on("/setHumidity", HTTP_POST, handleSetHumidity);
  server.on("/wifi", HTTP_GET, handleWiFiConfig);
  server.on("/wifi", HTTP_POST, handleWiFiSave);
  server.on("/update", HTTP_GET, []() {
    server.send(200, "text/html", update_html);
  });
  server.on("/update", HTTP_POST, []() {
    // POST handler is called after upload completes
    Serial.println("POST handler called - checking update status...");
    Serial.println("  isUpdating: " + String(isUpdating));
    Serial.println("  updateComplete: " + String(updateComplete));
    Serial.println("  Update.hasError(): " + String(Update.hasError()));
    
    // Check if update was successful
    if (Update.hasError()) {
      String errorMsg = "Update failed - Error code: " + String(Update.getError());
      Update.printError(Serial);
      Serial.println("Sending error response: " + errorMsg);
      server.send(500, "text/plain", errorMsg);
      isUpdating = false;
      updateComplete = false;
    } else if (updateComplete && !isUpdating) {
      // Upload completed successfully - verify before restarting
      Serial.println("Update completed successfully!");
      Serial.println("  Update size: " + String(Update.size()));
      Serial.println("  Update remaining: " + String(Update.remaining()));
      
      // Send success response
      Serial.println("Sending success response...");
      server.send(200, "text/plain", "Update successful! Device will restart...");
      
      // Flush any pending serial output
      Serial.flush();
      
      // Give the response time to send before restarting
      delay(1000);
      
      // Restart the device
      Serial.println("Restarting device NOW...");
      updateComplete = false;  // Reset flag
      delay(100);  // Small delay to ensure serial output is sent
      ESP.restart();
    } else if (isUpdating) {
      // Still uploading
      Serial.println("Upload still in progress...");
      server.send(200, "text/plain", "Upload in progress...");
    } else {
      // Upload not started or failed silently
      Serial.println("WARNING: Upload handler called but update not complete!");
      Serial.println("  This might indicate the upload didn't finish properly.");
      server.send(200, "text/plain", "Upload status unclear. Check serial output.");
    }
  });
  server.onFileUpload(handleUpdate);
  
  // Captive portal detection endpoints (for Android, iOS, Windows, etc.)
  server.on("/generate_204", HTTP_GET, []() {
    if (isAPMode) {
      server.sendHeader("Location", "/wifi", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(204, "text/plain", "");
    }
  });
  
  server.on("/hotspot-detect.html", HTTP_GET, []() {
    if (isAPMode) {
      server.sendHeader("Location", "/wifi", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    }
  });
  
  server.on("/success.txt", HTTP_GET, []() {
    if (isAPMode) {
      server.sendHeader("Location", "/wifi", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(200, "text/plain", "success");
    }
  });
  
  server.on("/kindle-wifi/wifiredirect.html", HTTP_GET, []() {
    if (isAPMode) {
      server.sendHeader("Location", "/wifi", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    }
  });
  
  // Catch-all handler for AP mode - redirect everything to WiFi config
  server.onNotFound([]() {
    if (isAPMode) {
      server.sendHeader("Location", "/wifi", true);
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });
  
  server.begin();
  
  // Start DNS server if in AP mode (will be started in startAPMode or here if already in AP mode)
  if (isAPMode) {
    IPAddress apIP = WiFi.softAPIP();
    dnsServer.start(DNS_PORT, "*", apIP);  // Redirect all DNS requests to AP IP
    Serial.println("DNS server started for captive portal");
  }
  
  // Show ready message
  showStatusMessage("Ready!", true);
  delay(1000);
  
  // Start main display
  updateDisplay();
}

void loop() {
  // Handle DNS server for captive portal (only in AP mode)
  if (isAPMode) {
    dnsServer.processNextRequest();
  }
  
  // Handle web server requests
  server.handleClient();
  
  // Check for update timeout
  checkUpdateTimeout();
  
  // Handle OTA updates (only if connected to WiFi, not in AP mode)
  if (!isAPMode && WiFi.status() == WL_CONNECTED) {
  ArduinoOTA.handle();
  }
  
  // Handle WiFi connection (only if not in AP mode)
  if (!isAPMode && WiFi.status() != WL_CONNECTED) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastWiFiReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
      lastWiFiReconnectAttempt = currentMillis;
      Serial.println("WiFi disconnected. Attempting to reconnect...");
      
      // Try to reconnect with saved credentials
      bool reconnected = false;
      if (wifiCredentialsValid) {
        reconnected = connectToWiFi(wifi_ssid, wifi_password, 10);
      }
      
      if (!reconnected) {
        // If all reconnection attempts failed, switch to AP mode
        Serial.println("Reconnection failed. Switching to AP mode...");
        startAPMode();
        // Start DNS server for captive portal
        IPAddress apIP = WiFi.softAPIP();
        dnsServer.start(DNS_PORT, "*", apIP);
        Serial.println("DNS server started for captive portal");
      }
    }
  }
  
  #if ENABLE_HOME_ASSISTANT
  // Handle MQTT
  if (!mqtt.connected()) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastMqttReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnectAttempt = currentMillis;
      reconnectMQTT();
    }
  } else {
    mqtt.loop();
    
    // Publish state every 5 seconds
    static unsigned long lastMqttPublish = 0;
    if (millis() - lastMqttPublish >= 5000) {
      publishMQTTState();
      lastMqttPublish = millis();
    }
  }
  #endif
  
  // Handle DNS server for captive portal (only in AP mode)
  if (isAPMode) {
    dnsServer.processNextRequest();
  }
  
  // Handle web server requests
  server.handleClient();
  
  // Read sensors
  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL) {
    readSensors();
    lastSensorRead = millis();
  }
  
  // Handle button input
  handleButton();
  
  // Check float switch
  checkFloatSwitch();
  
  // Update display with animations
  if (millis() - lastAnimationUpdate >= ANIMATION_UPDATE_INTERVAL) {
    updateDisplay();
    updateNeoPixel();
    lastAnimationUpdate = millis();
  }
  
  // Control dehumidifier
  controlDehumidifier();
  
  // Check update timeout
  checkUpdateTimeout();
}


// WiFi configuration handlers
// Pin configuration functions
void savePinConfiguration() {
  EEPROM.writeByte(FLOAT_SWITCH_PIN_ADDR, floatSwitchPin);
  EEPROM.writeByte(COMPRESSOR_RELAY_PIN_ADDR, compressorRelayPin);
  EEPROM.writeByte(FLOAT_SWITCH_INVERTED_ADDR, floatSwitchInverted ? 1 : 0);
  EEPROM.writeByte(PIN_CONFIG_VALID_ADDR, 1);
        EEPROM.commit();
  Serial.print("Pin configuration saved - Float Switch: GPIO ");
  Serial.print(floatSwitchPin);
  Serial.print(" (inverted: ");
  Serial.print(floatSwitchInverted ? "yes" : "no");
  Serial.print("), Compressor Relay: GPIO ");
  Serial.println(compressorRelayPin);
}

void loadPinConfiguration() {
  byte valid = EEPROM.readByte(PIN_CONFIG_VALID_ADDR);
  if (valid == 1) {
    floatSwitchPin = EEPROM.readByte(FLOAT_SWITCH_PIN_ADDR);
    compressorRelayPin = EEPROM.readByte(COMPRESSOR_RELAY_PIN_ADDR);
    floatSwitchInverted = (EEPROM.readByte(FLOAT_SWITCH_INVERTED_ADDR) == 1);
    Serial.print("Pin configuration loaded - Float Switch: GPIO ");
    Serial.print(floatSwitchPin);
    Serial.print(" (inverted: ");
    Serial.print(floatSwitchInverted ? "yes" : "no");
    Serial.print("), Compressor Relay: GPIO ");
    Serial.println(compressorRelayPin);
    } else {
    // Use defaults
    floatSwitchPin = FLOAT_SWITCH;
    compressorRelayPin = DEHUMIDIFIER_RELAY;
    floatSwitchInverted = false;
    Serial.println("Using default pin configuration");
  }
}

void handleWiFiConfig() {
  String html = "<!DOCTYPE html><html><head><title>WiFi Configuration</title>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>";
  html += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,Ubuntu,sans-serif;margin:0;padding:20px;background:#f5f5f5;color:#333}";
  html += ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}";
  html += "h1{color:#2c3e50;margin:0 0 20px 0;font-size:24px;text-align:center}";
  html += "h2{color:#34495e;font-size:18px;margin:25px 0 15px 0;border-bottom:2px solid #3498db;padding-bottom:5px}";
  html += ".form-group{margin-bottom:20px}";
  html += "label{display:block;margin-bottom:8px;font-weight:500;color:#2c3e50}";
  html += "input[type=\"text\"],input[type=\"password\"],input[type=\"number\"]{width:100%;padding:10px;border:1px solid #ddd;border-radius:4px;font-size:16px;box-sizing:border-box}";
  html += "input[type=\"number\"]{max-width:150px}";
  html += "button{background:#3498db;color:white;border:none;padding:12px 24px;border-radius:4px;cursor:pointer;font-size:16px;width:100%;transition:background 0.3s}";
  html += "button:hover{background:#2980b9}";
  html += ".info{background:#e8f4f8;padding:15px;border-radius:4px;margin-bottom:20px;color:#2c3e50}";
  html += ".status{text-align:center;margin-top:15px;padding:10px;border-radius:4px}";
  html += ".status.success{background:#dff0d8;color:#3c763d}";
  html += ".status.error{background:#f2dede;color:#a94442}";
  html += ".back-link{text-align:center;margin-top:20px}";
  html += ".back-link a{color:#3498db;text-decoration:none}";
  html += ".back-link a:hover{text-decoration:underline}";
  html += ".pin-info{font-size:12px;color:#666;margin-top:5px}";
  html += "</style></head><body>";
  html += "<div class=\"container\">";
  html += "<h1>Device Configuration</h1>";
  
  if (isAPMode) {
    html += "<div class=\"info\">";
    html += "<strong>Access Point Mode</strong><br>";
    html += "Configure your WiFi credentials and device pin settings.";
    html += "</div>";
  }
  
  html += "<form action=\"/wifi\" method=\"post\">";
  
  // WiFi Configuration Section
  html += "<h2>WiFi Settings</h2>";
  html += "<div class=\"form-group\">";
  html += "<label for=\"ssid\">WiFi Network Name (SSID):</label>";
  html += "<input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"" + wifi_ssid + "\" placeholder=\"Enter your WiFi network name\">";
  html += "</div>";
  html += "<div class=\"form-group\">";
  html += "<label for=\"password\">WiFi Password:</label>";
  html += "<input type=\"password\" id=\"password\" name=\"password\" placeholder=\"Leave empty to keep current password\">";
  html += "<div class=\"pin-info\" style=\"margin-top: 5px; font-size: 12px; color: #666;\">";
  html += "Leave password empty if you don't want to change WiFi settings";
  html += "</div>";
  html += "</div>";
  
  // Pin Configuration Section
  html += "<h2>Pin Configuration</h2>";
  html += "<div class=\"form-group\">";
  html += "<label for=\"float_switch_pin\">Float Switch Pin (Condensation Tank Monitor):</label>";
  html += "<input type=\"number\" id=\"float_switch_pin\" name=\"float_switch_pin\" value=\"" + String(floatSwitchPin) + "\" min=\"0\" max=\"48\" required>";
  html += "<div class=\"pin-info\">Current: GPIO " + String(floatSwitchPin) + " (Default: " + String(FLOAT_SWITCH) + ")</div>";
  html += "<div class=\"form-group\" style=\"margin-top: 10px;\">";
  html += "<label style=\"display: flex; align-items: center; gap: 8px;\">";
  html += "<input type=\"checkbox\" id=\"float_switch_inverted\" name=\"float_switch_inverted\" value=\"1\"" + String(floatSwitchInverted ? " checked" : "") + ">";
  html += "Invert float switch logic (tank full when HIGH instead of LOW)";
  html += "</label>";
  html += "<div class=\"pin-info\" style=\"margin-top: 5px; font-size: 12px; color: #666;\">";
  html += "Default: Tank full when pin is LOW (normally-open switch). Check this if your switch is normally-closed.";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "<div class=\"form-group\">";
  html += "<label for=\"compressor_relay_pin\">Compressor Relay Pin:</label>";
  html += "<input type=\"number\" id=\"compressor_relay_pin\" name=\"compressor_relay_pin\" value=\"" + String(compressorRelayPin) + "\" min=\"0\" max=\"48\" required>";
  html += "<div class=\"pin-info\">Current: GPIO " + String(compressorRelayPin) + " (Default: " + String(DEHUMIDIFIER_RELAY) + ")</div>";
  html += "</div>";
  
  html += "<button type=\"submit\">Save Configuration</button>";
  html += "</form>";
  
  if (wifiCredentialsValid) {
    html += "<div class=\"status success\">";
    html += "Saved WiFi SSID: " + wifi_ssid;
    html += "</div>";
  }
  
  html += "<div class=\"back-link\">";
  if (!isAPMode) {
    html += "<a href=\"/\">Back to Main Page</a>";
  }
  html += "</div>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleWiFiSave() {
  bool pinConfigChanged = false;
  bool wifiConfigChanged = false;
  
  // Handle pin configuration
  if (server.hasArg("float_switch_pin") && server.hasArg("compressor_relay_pin")) {
    uint8_t newFloatPin = server.arg("float_switch_pin").toInt();
    uint8_t newCompressorPin = server.arg("compressor_relay_pin").toInt();
    bool newInverted = server.hasArg("float_switch_inverted");
    
    // Validate pin numbers (ESP32-S2 has GPIO 0-46, but some are reserved)
    if (newFloatPin <= 48 && newCompressorPin <= 48 && 
        newFloatPin != newCompressorPin) {
      floatSwitchPin = newFloatPin;
      compressorRelayPin = newCompressorPin;
      floatSwitchInverted = newInverted;
      savePinConfiguration();
      pinConfigChanged = true;
      Serial.println("Pin configuration updated");
    }
  }
  
  // Handle WiFi credentials - only update if password is provided
  // If password is empty, assume user doesn't want to change WiFi settings
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String newSSID = server.arg("ssid");
    String newPassword = server.arg("password");
    
    // Only update WiFi if password is not empty
    if (newPassword.length() > 0 && newSSID.length() > 0 && newSSID.length() <= 32) {
      // Save credentials
      saveWiFiCredentials(newSSID, newPassword);
      wifi_ssid = newSSID;
      wifi_password = newPassword;
      wifiCredentialsValid = true;
      wifiConfigChanged = true;
      Serial.println("WiFi credentials updated");
    } else if (newPassword.length() == 0) {
      Serial.println("Password empty - skipping WiFi update (assuming no change desired)");
    }
  }
  
  // If pins changed, always restart to apply changes
  if (pinConfigChanged) {
    String html = "<!DOCTYPE html><html><head><title>Configuration Saved</title>";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<meta http-equiv=\"refresh\" content=\"3;url=/wifi\">";
    html += "<style>";
    html += "body{font-family:Arial;text-align:center;padding:50px;background:#f5f5f5}";
    html += ".container{max-width:400px;margin:0 auto;background:white;padding:30px;border-radius:10px}";
    html += "h1{color:#2c3e50}";
    html += "</style></head><body>";
    html += "<div class=\"container\">";
    html += "<h1>Configuration Saved</h1>";
    html += "<p>Pin configuration updated. Device will restart to apply changes.</p>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
    delay(1000);
    ESP.restart();
    return;
  }
  
  // If WiFi config changed, try to connect
  if (wifiConfigChanged && server.hasArg("ssid")) {
    String newSSID = server.arg("ssid");
    String newPassword = server.hasArg("password") ? server.arg("password") : "";
    
    Serial.println("Attempting to connect with new credentials...");
    
    String html = "<!DOCTYPE html><html><head><title>Connecting...</title>";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<meta http-equiv=\"refresh\" content=\"5;url=/\">";
    html += "<style>";
    html += "body{font-family:Arial;text-align:center;padding:50px;background:#f5f5f5}";
    html += ".container{max-width:400px;margin:0 auto;background:white;padding:30px;border-radius:10px}";
    html += "h1{color:#2c3e50}";
    html += ".spinner{border:4px solid #f3f3f3;border-top:4px solid #3498db;border-radius:50%;width:40px;height:40px;animation:spin 1s linear infinite;margin:20px auto}";
    html += "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}";
    html += "</style></head><body>";
    html += "<div class=\"container\">";
    html += "<h1>Connecting to WiFi...</h1>";
    html += "<div class=\"spinner\"></div>";
    html += "<p>Please wait while the device connects to your WiFi network.</p>";
    html += "<p>The device will restart automatically.</p>";
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
    
    // Give the response time to send
    delay(1000);
    
    // Try to connect
    bool connected = connectToWiFi(newSSID, newPassword, 15);
    
    if (connected) {
      // Success - restart to fully initialize
      delay(2000);
      ESP.restart();
      } else {
      // Failed - stay in AP mode
      Serial.println("Connection failed. Remaining in AP mode.");
    }
  } else if (!wifiConfigChanged && !pinConfigChanged) {
    server.send(400, "text/plain", "No configuration changes detected");
  }
}

// Web server handlers
void handleRoot() {
  // If in AP mode, redirect to WiFi config
  if (isAPMode) {
    server.sendHeader("Location", "/wifi", true);
    server.send(302, "text/plain", "");
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    server.send(503, "text/plain", "Service Unavailable - WiFi Disconnected");
    return;
  }
  
  String html = "<!DOCTYPE html><html><head><title>Dehumidifier Controller</title>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>";
  html += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,Ubuntu,sans-serif;margin:0;padding:20px;background:#f5f5f5;color:#333}";
  html += ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}";
  html += "h1{color:#2c3e50;margin:0 0 20px 0;font-size:24px;text-align:center}";
  html += ".status-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:15px;margin-bottom:20px}";
  html += ".status-item{background:#f8f9fa;padding:15px;border-radius:8px;text-align:center}";
  html += ".status-item.humidity{grid-column:1/-1;background:#e8f4f8;padding:25px;margin-bottom:10px}";
  html += ".status-label{font-size:14px;color:#666;margin-bottom:5px}";
  html += ".status-item.humidity .status-label{font-size:16px;color:#2c3e50;font-weight:500}";
  html += ".status-value{font-size:24px;font-weight:bold;color:#2c3e50}";
  html += ".status-item.humidity .status-value{font-size:48px;color:#2980b9;margin:10px 0}";
  html += ".status-value.running{color:#27ae60}";
  html += ".status-value.standby{color:#7f8c8d}";
  html += ".status-value.full{color:#e74c3c}";
  html += ".status-value.tank-ok{color:#27ae60}";
  html += ".status-value.tank-full{color:#e74c3c}";
  html += ".control-panel{background:#f8f9fa;padding:20px;border-radius:8px;margin-top:20px}";
  html += ".humidity-control{display:flex;align-items:center;gap:10px;margin-bottom:15px}";
  html += "input[type=\"number\"]{width:80px;padding:8px;border:1px solid #ddd;border-radius:4px;font-size:16px}";
  html += "button{background:#3498db;color:white;border:none;padding:10px 20px;border-radius:4px;cursor:pointer;font-size:14px;transition:background 0.3s}";
  html += "button:hover{background:#2980b9}";
  html += ".links{margin-top:20px;text-align:center}";
  html += ".links a{color:#3498db;text-decoration:none;margin:0 10px}";
  html += ".links a:hover{text-decoration:underline}";
  html += ".wifi-status{text-align:center;margin-top:15px;font-size:14px;color:#666}";
  html += ".refresh-button{background:#3498db;color:white;border:none;padding:8px 16px;border-radius:4px;cursor:pointer;font-size:14px;transition:background 0.3s;margin-top:10px}";
  html += ".refresh-button:hover{background:#2980b9}";
  html += ".cooldown-timer{color:#e67e22;font-size:14px;margin-top:5px}";
  html += ".version-info{text-align:center;margin-top:10px;font-size:12px;color:#666}";
  html += "</style></head><body>";
  html += "<div class=\"container\">";
  html += "<h1>Dehumidifier Controller</h1>";
  html += "<div class=\"status-grid\">";
  html += "<div class=\"status-item humidity\">";
  html += "<div class=\"status-label\">Current Humidity</div>";
  html += "<div class=\"status-value\">" + String(currentHumidity, 1) + "%</div></div>";
  html += "<div class=\"status-item\">";
  html += "<div class=\"status-label\">Temperature</div>";
  html += "<div class=\"status-value\">" + String(currentTemperature, 1) + "°C</div></div>";
  html += "<div class=\"status-item\">";
  html += "<div class=\"status-label\">Status</div>";
  html += "<div class=\"status-value " + String(compressorInCooldown ? "cooldown" : (dehumidifierRunning ? "running" : "standby")) + "\">";
  html += String(compressorInCooldown ? "Cooldown" : (dehumidifierRunning ? "Running" : "Standby")) + "</div>";
  if (compressorInCooldown) {
    unsigned long remaining = getCooldownRemaining() / 1000;  // Convert to seconds
    int minutes = remaining / 60;
    int seconds = remaining % 60;
    html += "<div class=\"cooldown-timer\">" + String(minutes) + "m " + String(seconds) + "s remaining</div>";
  }
  html += "</div>";
  html += "<div class=\"status-item\">";
  html += "<div class=\"status-label\">Condensation Tank</div>";
  html += "<div class=\"status-value " + String(floatSwitchTriggered ? "tank-full" : "tank-ok") + "\">";
  html += String(floatSwitchTriggered ? "FULL" : "OK") + "</div></div></div>";
  html += "<div class=\"control-panel\">";
  html += "<form action=\"/setHumidity\" method=\"post\">";
  html += "<div class=\"humidity-control\">";
  html += "<label for=\"humidity\">Target Humidity:</label>";
  html += "<input type=\"number\" name=\"humidity\" id=\"humidity\" min=\"30\" max=\"70\" step=\"5\" value=\"" + String(targetHumidity) + "\">";
  html += "<button type=\"submit\">Set</button></div></form></div>";
  html += "<div class=\"links\">";
  html += "<a href=\"/wifi\">Settings</a>";
  html += "<a href=\"/update\">Update Firmware</a>";
  html += "<button type=\"button\" onclick=\"window.location.reload()\" class=\"refresh-button\">Refresh</button>";
  html += "</div>";
  html += "<div class=\"wifi-status\">IP: " + WiFi.localIP().toString() + "</div>";
  html += "<div class=\"version-info\">";
  html += "Version " + getVersionString() + "<br>";
  html += "Build: " + String(VERSION_BUILD);
  html += "</div>";
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleData() {
  if (isAPMode) {
    server.send(200, "application/json", "{\"error\":\"AP Mode - WiFi configuration required\",\"ap_mode\":true,\"ap_ssid\":\"" + String(AP_SSID) + "\",\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\"}");
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    server.send(503, "application/json", "{\"error\":\"WiFi Disconnected\"}");
    return;
  }
  
  String json = "{";
  json += "\"version\":\"" + getVersionString() + "\",";
  json += "\"build\":\"" + String(VERSION_BUILD) + "\",";
  json += "\"temperature\":" + String(currentTemperature) + ",";
  json += "\"humidity\":" + String(currentHumidity) + ",";
  json += "\"targetHumidity\":" + String(targetHumidity) + ",";
  json += "\"running\":" + String(dehumidifierRunning) + ",";
  json += "\"cooldown\":" + String(compressorInCooldown) + ",";
  json += "\"cooldownRemaining\":" + String(getCooldownRemaining() / 1000) + ",";
  json += "\"drainFull\":" + String(floatSwitchTriggered);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetHumidity() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(503, "text/plain", "Service Unavailable - WiFi Disconnected");
    return;
  }
  
  if (server.hasArg("humidity")) {
    float newHumidity = server.arg("humidity").toFloat();
    if (newHumidity >= 30 && newHumidity <= 70 && fmod(newHumidity, 5) == 0) {
      targetHumidity = newHumidity;
      // Save to EEPROM
      EEPROM.writeFloat(TARGET_HUMIDITY_ADDR, targetHumidity);
      if (EEPROM.commit()) {
        Serial.print("Successfully saved new target humidity: ");
        Serial.println(targetHumidity);
      } else {
        Serial.println("Failed to save target humidity to EEPROM");
      }
    }
  }
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// Handle firmware update upload
void handleUpdate() {
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    isUpdating = true;
    updateComplete = false;  // Reset completion flag
    updateStartTime = millis();
    
    Serial.println("Update start: " + upload.filename);
    
    // Clear display and show prominent update message
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_CYAN);
    tft.setTextSize(3);
    tft.setCursor(20, 40);
    tft.println("FIRMWARE");
    tft.setCursor(30, 70);
    tft.println("UPDATE");
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 100);
    tft.println("Please wait...");
    
    // Start OTA update - use U_FLASH to update the sketch
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    Serial.println("Max sketch space: " + String(maxSketchSpace));
    Serial.println("Free heap: " + String(ESP.getFreeHeap()));
    
    // Begin update with U_FLASH type
    if (!Update.begin(maxSketchSpace, U_FLASH)) {
      Update.printError(Serial);
      Serial.println("Update.begin() failed");
      Serial.print("Error code: ");
      Serial.println(Update.getError());
      isUpdating = false;
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_RED);
      tft.setTextSize(2);
      tft.setCursor(10, 50);
      tft.println("Update");
      tft.setCursor(10, 70);
      tft.println("Failed!");
      return;
    }
    Serial.println("Update.begin() successful - ready to receive firmware");
    
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Write data chunk
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
      Serial.println("Update.write() failed");
      isUpdating = false;
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_RED);
      tft.setTextSize(2);
      tft.setCursor(10, 50);
      tft.println("Write");
      tft.setCursor(10, 70);
      tft.println("Failed!");
      return;
    }
    
    // Show progress on display - update more frequently
    if (upload.totalSize > 0) {
      int progress = (upload.currentSize * 100) / upload.totalSize;
    int barWidth = 200;
      int barHeight = 25;
    int x = (240 - barWidth) / 2;
      int y = 110;
    
      // Clear and redraw progress bar with percentage
      tft.fillRect(x, y, barWidth, barHeight + 20, ST77XX_BLACK);
    tft.drawRect(x, y, barWidth, barHeight, ST77XX_WHITE);
      int progressWidth = ((barWidth - 2) * progress) / 100;
      tft.fillRect(x + 1, y + 1, progressWidth, barHeight - 2, ST77XX_GREEN);
      
      // Show percentage text
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(2);
      tft.setCursor(x + barWidth/2 - 20, y + barHeight + 5);
      tft.print(String(progress) + "%");
      
      Serial.println("Progress: " + String(progress) + "%");
    }
    
  } else if (upload.status == UPLOAD_FILE_END) {
    Serial.println("=== UPLOAD_FILE_END ===");
    Serial.println("Total size expected: " + String(upload.totalSize));
    Serial.println("Current size received: " + String(upload.currentSize));
    Serial.println("Update.size() so far: " + String(Update.size()));
    Serial.println("Update.remaining(): " + String(Update.remaining()));
    
    // Verify we received the complete file
    if (upload.totalSize > 0 && upload.currentSize != upload.totalSize) {
      Serial.println("ERROR: Size mismatch! Expected: " + String(upload.totalSize) + ", Got: " + String(upload.currentSize));
      Update.abort();
      isUpdating = false;
      updateComplete = false;
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_RED);
      tft.setTextSize(2);
      tft.setCursor(10, 50);
      tft.println("Size");
      tft.setCursor(10, 70);
      tft.println("Mismatch!");
      return;
    }
    
    // Finalize and commit the update
    // The 'true' parameter commits the update even if size doesn't match exactly
    // But we've already checked for size mismatch above
    Serial.println("Calling Update.end(true) to finalize and commit firmware...");
    bool endResult = Update.end(true);
    
    Serial.println("Update.end() returned: " + String(endResult ? "true" : "false"));
    
    if (endResult) {
      Serial.println("=== UPDATE SUCCESS ===");
      Serial.println("Firmware committed successfully!");
      Serial.println("Final size written: " + String(Update.size()));
      Serial.println("Free sketch space after update: " + String(ESP.getFreeSketchSpace()));
      
      isUpdating = false;  // Mark as complete
      updateComplete = true;  // Set flag for POST handler
      
      // Show completion message
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_GREEN);
      tft.setTextSize(2);
      tft.setCursor(10, 50);
      tft.println("Update");
      tft.setCursor(10, 70);
      tft.println("Complete!");
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(1);
      tft.setCursor(10, 95);
      tft.println("Restarting...");
    } else {
      Update.printError(Serial);
      Serial.println("=== UPDATE FAILED ===");
      Serial.println("Update.end() FAILED - firmware NOT committed!");
      Serial.print("Error code: ");
      Serial.println(Update.getError());
      
      isUpdating = false;
      updateComplete = false;
      
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_RED);
      tft.setTextSize(2);
      tft.setCursor(10, 50);
      tft.println("Update");
      tft.setCursor(10, 70);
      tft.println("Failed!");
      tft.setTextSize(1);
      tft.setCursor(10, 95);
      tft.println("Check serial");
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Serial.println("Update aborted");
    Update.abort();
    isUpdating = false;
    
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.println("Update");
    tft.setCursor(10, 70);
    tft.println("Aborted!");
  }
}

// Add this to your loop() function
void checkUpdateTimeout() {
  if (isUpdating && (millis() - updateStartTime > UPDATE_TIMEOUT)) {
    isUpdating = false;
    Update.abort();
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.println("Update");
    tft.setCursor(10, 70);
    tft.println("Timeout!");
    delay(2000);
  }
} 