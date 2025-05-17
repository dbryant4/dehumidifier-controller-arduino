#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_SHTC3.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <EEPROM.h>

// Add after the includes and before the configuration flags
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0
#define VERSION_BUILD __DATE__ " " __TIME__

// Configuration flags
#define ENABLE_HOME_ASSISTANT true  // Set to true to enable Home Assistant integration

#if ENABLE_HOME_ASSISTANT
#include <PubSubClient.h>  // Only include MQTT library if Home Assistant is enabled
#endif

// Pin definitions
#define TFT_CS        7
#define TFT_RST       40
#define TFT_DC        39
#define TFT_BACKLITE  45  // Backlight pin
#define TFT_I2C_POWER 21 // Power pin for TFT
#define BOOT_BUTTON 0  // Use BOOT button (GPIO 0)
#define DEHUMIDIFIER_RELAY 13
#define FLOAT_SWITCH 12
// #define NEOPIXEL_PIN 1  // Built-in NeoPixel pin

// EEPROM settings
#define EEPROM_SIZE 512
#define TARGET_HUMIDITY_ADDR 0
#define LAST_COMPRESSOR_OFF_ADDR 4  // 4 bytes for timestamp
#define COMPRESSOR_COOLDOWN_FLAG_ADDR 8  // 1 byte for cooldown flag

// Display setup
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
Adafruit_SHTC3 shtc3 = Adafruit_SHTC3();
Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Sensor type tracking
bool usingSHT4x = false;
bool usingSHTC3 = false;

// WiFi credentials
const char* ssid = "zippy-iot";
const char* password = "creepy8laburnum*cabaret3RETAIL";

// OTA settings
const char* hostname = "dehumidifier-controller";

#if ENABLE_HOME_ASSISTANT
// MQTT settings
const char* mqtt_server = "192.168.1.3";  // Change this to your MQTT broker address
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

// Global variables
float targetHumidity = 50.0;  // Default target humidity
float currentHumidity = 0.0;
float currentTemperature = 0.0;
bool dehumidifierRunning = false;
bool floatSwitchTriggered = false;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_READ_INTERVAL = 2000;  // Read sensors every 2 seconds
unsigned long lastAnimationUpdate = 0;
const unsigned long ANIMATION_UPDATE_INTERVAL = 20;  // Reduced from 50ms to 20ms for smoother animations
bool animationState = false;

// Add these global variables at the top with other globals
float prevTemperature = -999;
float prevHumidity = -999;
float prevTargetHumidity = -999;
bool prevDehumidifierRunning = false;
bool prevFloatSwitchTriggered = false;
bool prevWiFiConnected = false;
String prevIP = "";

// Add these global variables at the top with other globals
unsigned long lastWiFiReconnectAttempt = 0;
const unsigned long WIFI_RECONNECT_INTERVAL = 5000; // Try to reconnect every 5 seconds

// Add these global variables at the top with other globals
unsigned long lastNeoPixelUpdate = 0;
const unsigned long NEO_PIXEL_UPDATE_INTERVAL = 10;  // Update every 10ms for smoother fade
int neoPixelBrightness = 0;
bool neoPixelFadeDirection = true;  // true = fade in, false = fade out

// Web server
WebServer server(80);

// Add these global variables at the top with other globals
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_UPDATE_INTERVAL = 5;  // Reduced from 10ms to 5ms for more frequent updates

// Add these global variables at the top with other globals
bool isUpdating = false;
unsigned long updateStartTime = 0;
const unsigned long UPDATE_TIMEOUT = 300000; // 5 minutes timeout for updates

// Add these global variables at the top with other globals
unsigned long lastButtonPress = 0;
unsigned long lastButtonRelease = 0;
bool buttonPressed = false;
bool buttonHandled = false;
const unsigned long DEBOUNCE_TIME = 50;  // Debounce time in milliseconds
const unsigned long LONG_PRESS_TIME = 2000;  // Long press threshold in milliseconds

// Add these global variables at the top with other globals
unsigned long lastCompressorOff = 0;
const unsigned long COMPRESSOR_MIN_OFF_TIME = 300000;  // 5 minutes in milliseconds
bool compressorInCooldown = false;

// Add these global variables at the top with other globals
unsigned long lastStatusBarUpdate = 0;
const unsigned long STATUS_BAR_UPDATE_INTERVAL = 1000;  // Update status bar every 1 second

// Add this HTML template at the top of the file after includes
const char* update_html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Dehumidifier Controller - Update Firmware</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 20px; }
        .container { max-width: 600px; margin: 0 auto; }
        .progress { width: 100%; background-color: #f0f0f0; border-radius: 4px; }
        .progress-bar { height: 20px; background-color: #4CAF50; border-radius: 4px; width: 0%; }
        .status { margin-top: 20px; padding: 10px; border-radius: 4px; }
        .success { background-color: #dff0d8; color: #3c763d; }
        .error { background-color: #f2dede; color: #a94442; }
        .hidden { display: none; }
        button { padding: 10px 20px; background-color: #4CAF50; color: white; border: none; border-radius: 4px; cursor: pointer; }
        button:disabled { background-color: #cccccc; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Update Firmware</h1>
        <form id="uploadForm" action="/update" method="post" enctype="multipart/form-data">
            <input type="file" name="firmware" accept=".bin">
            <button type="submit" id="uploadButton">Upload Firmware</button>
        </form>
        <div id="progress" class="progress hidden">
            <div id="progressBar" class="progress-bar"></div>
        </div>
        <div id="status" class="status hidden"></div>
        <p><a href="/">Back to Main Page</a></p>
    </div>
    <script>
        document.getElementById('uploadForm').onsubmit = function(e) {
            e.preventDefault();
            var formData = new FormData(this);
            var xhr = new XMLHttpRequest();
            var progress = document.getElementById('progress');
            var progressBar = document.getElementById('progressBar');
            var status = document.getElementById('status');
            var uploadButton = document.getElementById('uploadButton');
            
            progress.classList.remove('hidden');
            status.classList.remove('hidden');
            uploadButton.disabled = true;
            
            xhr.upload.onprogress = function(e) {
                if (e.lengthComputable) {
                    var percentComplete = (e.loaded / e.total) * 100;
                    progressBar.style.width = percentComplete + '%';
                }
            };
            
            xhr.onload = function() {
                if (xhr.status === 200) {
                    status.textContent = 'Update successful! Device will restart...';
                    status.className = 'status success';
                    setTimeout(function() {
                        window.location.href = '/';
                    }, 5000);
                } else {
                    status.textContent = 'Update failed: ' + xhr.responseText;
                    status.className = 'status error';
                    uploadButton.disabled = false;
                }
            };
            
            xhr.onerror = function() {
                status.textContent = 'Update failed: Network error';
                status.className = 'status error';
                uploadButton.disabled = false;
            };
            
            xhr.open('POST', '/update', true);
            xhr.send(formData);
        };
    </script>
</body>
</html>
)";

// Add this helper function after the global variables
int getWiFiSignalStrength() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  int rssi = WiFi.RSSI();
  // Convert RSSI to percentage (RSSI typically ranges from -100 to -30)
  // -30 is excellent signal, -100 is poor signal
  int percentage = map(rssi, -100, -30, 0, 100);
  return constrain(percentage, 0, 100);
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
  lastCompressorOff = EEPROM.readULong(LAST_COMPRESSOR_OFF_ADDR);
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
  pixel.begin();
  pixel.setBrightness(50);
  pixel.show();
  
  // Initialize display
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);
  
  tft.init(135, 240);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
  
  // Initialize button and outputs
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  pinMode(DEHUMIDIFIER_RELAY, OUTPUT);
  pinMode(FLOAT_SWITCH, INPUT_PULLUP);
  
  // Initialize sensors
  if (sht4.begin()) {
    usingSHT4x = true;
  } else if (shtc3.begin()) {
    usingSHTC3 = true;
  } else {
    Serial.println("ERROR: No supported sensor found!");
    while (1) delay(1);
  }
  
  // Connect to WiFi
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(15, 10);
  tft.println("Connecting to");
  tft.setCursor(15, 30);
  tft.println("WiFi...");
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }
  
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  // Setup OTA
  setupOTA();
  
  #if ENABLE_HOME_ASSISTANT
  // Setup MQTT
  setupMQTT();
  #endif
  
  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/json", HTTP_GET, handleData);
  server.on("/setHumidity", HTTP_POST, handleSetHumidity);
  server.on("/update", HTTP_GET, []() {
    server.send(200, "text/html", update_html);
  });
  server.on("/update", HTTP_POST, []() {
    server.send(200, "text/plain", "Update started");
  }, handleUpdate);
  server.begin();
  
  updateDisplay();
}

void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();
  
  // Handle WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastWiFiReconnectAttempt >= WIFI_RECONNECT_INTERVAL) {
      lastWiFiReconnectAttempt = currentMillis;
      Serial.println("WiFi disconnected. Attempting to reconnect...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
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
  
  server.handleClient();
  
  // Read sensors every 5 seconds
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
    updateNeoPixel();  // Update NeoPixel status
    lastAnimationUpdate = millis();
  }
  
  // Control dehumidifier
  controlDehumidifier();
  
  // Check update timeout
  checkUpdateTimeout();
}

void readSensors() {
  sensors_event_t humidity, temp;
  
  if (usingSHT4x) {
    sht4.getEvent(&humidity, &temp);
  } else if (usingSHTC3) {
    shtc3.getEvent(&humidity, &temp);
  }
  
  currentHumidity = humidity.relative_humidity;
  currentTemperature = temp.temperature;
}

void handleButton() {
  bool currentState = digitalRead(BOOT_BUTTON);
  unsigned long currentTime = millis();
  
  currentState = !currentState;
  
  if (currentState && !buttonPressed) {
    if (currentTime - lastButtonRelease > DEBOUNCE_TIME) {
      buttonPressed = true;
      lastButtonPress = currentTime;
      buttonHandled = false;
    }
  }
  
  if (!currentState && buttonPressed) {
    if (currentTime - lastButtonPress > DEBOUNCE_TIME) {
      buttonPressed = false;
      lastButtonRelease = currentTime;
      
      if (!buttonHandled) {
        targetHumidity += 5;
        if (targetHumidity > 70) targetHumidity = 30;
        buttonHandled = true;
      }
    }
  }
}

void checkFloatSwitch() {
  bool newState = (digitalRead(FLOAT_SWITCH) == LOW);
  if (newState != floatSwitchTriggered) {
    floatSwitchTriggered = newState;
    Serial.println(floatSwitchTriggered ? "DRAIN FULL!" : "Drain OK");
  }
}

void controlDehumidifier() {
  if (!floatSwitchTriggered) {
    unsigned long currentMillis = millis();
    
    // Check if compressor is in cooldown period
    if (!dehumidifierRunning && currentMillis - lastCompressorOff < COMPRESSOR_MIN_OFF_TIME) {
      // Still in cooldown period
      if (!compressorInCooldown) {
        compressorInCooldown = true;
        EEPROM.writeBool(COMPRESSOR_COOLDOWN_FLAG_ADDR, true);
        EEPROM.commit();
      }
    } else {
      if (compressorInCooldown) {
        compressorInCooldown = false;
        EEPROM.writeBool(COMPRESSOR_COOLDOWN_FLAG_ADDR, false);
        EEPROM.commit();
      }
      
      bool shouldRun = (currentHumidity > targetHumidity + 1);
      
      if (shouldRun != dehumidifierRunning) {
        if (shouldRun && !compressorInCooldown) {
          // Only start if not in cooldown
          dehumidifierRunning = true;
          digitalWrite(DEHUMIDIFIER_RELAY, HIGH);
          Serial.println("Dehumidifier STARTED");
        } else if (!shouldRun) {
          // Always allow stopping
          dehumidifierRunning = false;
          digitalWrite(DEHUMIDIFIER_RELAY, LOW);
          lastCompressorOff = currentMillis;
          EEPROM.writeULong(LAST_COMPRESSOR_OFF_ADDR, lastCompressorOff);
          EEPROM.commit();
          Serial.println("Dehumidifier STOPPED");
        }
      }
    }
  }
}

void updateDisplay() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL) {
    return;  // Don't update too frequently
  }
  lastDisplayUpdate = currentMillis;

  // Only clear the screen on first run
  static bool firstRun = true;
  if (firstRun) {
    tft.fillScreen(ST77XX_BLACK);
    firstRun = false;
  }

  // Define display regions
  const int STATUS_BAR_HEIGHT = 20;
  const int IP_BAR_HEIGHT = 20;
  const int MAIN_CONTENT_HEIGHT = 115;
  
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
  
  unsigned long currentMillis = millis();
  if (currentMillis - lastStatusBarUpdate < STATUS_BAR_UPDATE_INTERVAL) {
    return;  // Don't update too frequently
  }
  
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  bool needsUpdate = (wifiConnected != prevWifiConnected || 
                     dehumidifierRunning != prevDehumidifierRunning || 
                     floatSwitchTriggered != prevFloatSwitchTriggered ||
                     compressorInCooldown != prevCompressorInCooldown ||
                     WiFi.RSSI() != prevRSSI);  // Add RSSI change check
  
  if (needsUpdate) {
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
    uint16_t wifiColor = wifiConnected ? ST77XX_BLACK : ST77XX_RED;
    tft.fillRect(segmentWidth * 2, 0, segmentWidth, 20, wifiColor);
    tft.setTextColor(wifiConnected ? ST77XX_WHITE : ST77XX_WHITE);
    
    if (wifiConnected) {
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
        activeBars = 4;  // Excellent signal - all bars
        barColor = ST77XX_GREEN;  // Green for excellent signal
      } else if (signalStrength >= 50) {
        activeBars = 3;  // Good signal - 3 bars
        barColor = 0x07E0;  // Yellow-green for good signal
      } else if (signalStrength >= 25) {
        activeBars = 2;  // Fair signal - 2 bars
        barColor = 0xFFE0;  // Yellow for fair signal
      } else {
        activeBars = 1;  // Poor signal - 1 bar
        barColor = 0xFD20;  // Orange for poor signal
      }
      
      // Draw bars with consistent bottom alignment
      for (int i = 0; i < numBars; i++) {
        int barX = x + (i * (barWidth / numBars));
        int barY = y + (barHeight * (numBars - i - 1));
        uint16_t currentBarColor = (i < activeBars) ? barColor : 0x7BEF;
        
        // Draw each bar with consistent height and bottom alignment
        tft.fillRect(barX, barY, (barWidth / numBars) - 1, barHeight, currentBarColor);
      }
      
      // Draw RSSI value
      int rssi = WiFi.RSSI();
      // Clear the area where RSSI will be displayed
      tft.fillRect(x + barWidth + 2, y, 40, 16, wifiColor);
      
      // Draw RSSI value in small font
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
  
  // Only clear the entire area on first run
  if (firstRun) {
    tft.fillRect(0, 45, 240, 90, ST77XX_BLACK);
    firstRun = false;
  }
  
  // Update humidity if changed (comparing rounded values)
  float roundedCurrentHumidity = round(currentHumidity * 10.0) / 10.0;
  float roundedPrevHumidity = round(prevHumidity * 10.0) / 10.0;
  
  if (roundedCurrentHumidity != roundedPrevHumidity) {
    // Clear the entire humidity area with extra width and height
    tft.fillRect(0, 15, 200, 55, ST77XX_BLACK);
    
    // Use smoother font for humidity value
    tft.setFont(&FreeSansBold18pt7b);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(15, 65);
    tft.print(currentHumidity, 1);
    
    // Switch to smaller font for percentage
    //tft.setFont(&FreeMonoBold9pt7b);
    // tft.setTextSize(2);
    tft.print("%");
    prevHumidity = currentHumidity;
  }
  
  // Update target humidity if changed
  if (targetHumidity != prevTargetHumidity) {
    // Clear only the target humidity area
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
  
  // Draw separator line between humidity and temperature
  tft.drawLine(10, 92, 230, 92, ST77XX_WHITE);
  
  // Update temperature if changed
  if (currentTemperature != prevTemperature) {
    // Clear the temperature area
    tft.fillRect(0, 97, 240, 20, ST77XX_BLACK);
    
    tft.setFont(&FreeMonoBold9pt7b);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    
    // Draw temperature value
    tft.setCursor(15, 112);
    tft.print(currentTemperature, 1);
    tft.print("C");
    
    // Draw Fahrenheit in smaller text
    float tempF = (currentTemperature * 9.0 / 5.0) + 32.0;
    tft.setCursor(80, 112);
    tft.print("(");
    tft.print(tempF, 1);
    tft.print("F)");
    prevTemperature = currentTemperature;
  }
  
  // Draw separator line between temperature and IP
  tft.drawLine(10, 117, 230, 117, ST77XX_WHITE);
  
  // Always show IP address at the bottom
  static String lastIP = "";
  String currentIP = WiFi.localIP().toString();
  
  if (currentIP != lastIP) {
    // Clear the IP area
    tft.fillRect(0, 122, 240, 15, ST77XX_BLACK);
    
    tft.setFont(&FreeMonoBold9pt7b);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(15, 132);
    tft.print("IP: ");
    tft.print(currentIP);
    lastIP = currentIP;
  }
}

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

// Web server handlers
void handleRoot() {
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
  html += "</div></div>";
  html += "<div class=\"control-panel\">";
  html += "<form action=\"/setHumidity\" method=\"post\">";
  html += "<div class=\"humidity-control\">";
  html += "<label for=\"humidity\">Target Humidity:</label>";
  html += "<input type=\"number\" name=\"humidity\" id=\"humidity\" min=\"30\" max=\"70\" step=\"5\" value=\"" + String(targetHumidity) + "\">";
  html += "<button type=\"submit\">Set</button></div></form></div>";
  html += "<div class=\"links\">";
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

// Add this function to handle the firmware update
void handleUpdate() {
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    isUpdating = true;
    updateStartTime = millis();
    
    // Clear display and show update message
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.println("Updating...");
    tft.setCursor(10, 70);
    tft.println("Please wait");
    
    // Start OTA update
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {
      Update.printError(Serial);
      server.send(500, "text/plain", "Update failed to begin");
      isUpdating = false;
      return;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
      server.send(500, "text/plain", "Update write failed");
      isUpdating = false;
      return;
    }
    
    // Show progress
    int progress = (upload.totalSize * 100) / upload.currentSize;
    int barWidth = 200;
    int barHeight = 20;
    int x = (240 - barWidth) / 2;
    int y = 100;
    
    tft.fillRect(x, y, barWidth, barHeight, ST77XX_BLACK);
    tft.drawRect(x, y, barWidth, barHeight, ST77XX_WHITE);
    tft.fillRect(x + 1, y + 1, (barWidth - 2) * progress / 100, barHeight - 2, ST77XX_GREEN);
    
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_GREEN);
      tft.setTextSize(2);
      tft.setCursor(10, 50);
      tft.println("Update");
      tft.setCursor(10, 70);
      tft.println("Complete!");
      delay(2000);
      ESP.restart();
    } else {
      Update.printError(Serial);
      server.send(500, "text/plain", "Update failed");
      isUpdating = false;
      
      tft.fillScreen(ST77XX_BLACK);
      tft.setTextColor(ST77XX_RED);
      tft.setTextSize(2);
      tft.setCursor(10, 50);
      tft.println("Update");
      tft.setCursor(10, 70);
      tft.println("Failed!");
      delay(2000);
    }
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

// Add this function after the other helper functions
unsigned long getCooldownRemaining() {
  if (!compressorInCooldown) return 0;
  unsigned long currentMillis = millis();
  unsigned long elapsed = currentMillis - lastCompressorOff;
  if (elapsed >= COMPRESSOR_MIN_OFF_TIME) return 0;
  return COMPRESSOR_MIN_OFF_TIME - elapsed;
}

// Add this helper function after the other helper functions
String getVersionString() {
  return String(VERSION_MAJOR) + "." + String(VERSION_MINOR) + "." + String(VERSION_PATCH);
} 