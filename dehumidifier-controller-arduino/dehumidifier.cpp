#include "dehumidifier.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <math.h>
#include "config.h"

// Dehumidifier state variables
bool dehumidifierRunning = false;
bool floatSwitchTriggered = false;
bool compressorInCooldown = false;
unsigned long lastCompressorOff = 0;

extern float targetHumidity;
extern float currentHumidity;
extern uint8_t floatSwitchPin;
extern uint8_t compressorRelayPin;

void checkFloatSwitch() {
  bool newState = (digitalRead(floatSwitchPin) == LOW);
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
          digitalWrite(compressorRelayPin, HIGH);
          Serial.println("Dehumidifier STARTED");
        } else if (!shouldRun) {
          // Always allow stopping
          dehumidifierRunning = false;
          digitalWrite(compressorRelayPin, LOW);
          lastCompressorOff = currentMillis;
          EEPROM.writeULong(LAST_COMPRESSOR_OFF_ADDR, lastCompressorOff);
          EEPROM.commit();
          Serial.println("Dehumidifier STOPPED");
        }
      }
    }
  }
}

unsigned long getCooldownRemaining() {
  if (!compressorInCooldown) return 0;
  unsigned long currentMillis = millis();
  unsigned long elapsed = currentMillis - lastCompressorOff;
  if (elapsed >= COMPRESSOR_MIN_OFF_TIME) return 0;
  return COMPRESSOR_MIN_OFF_TIME - elapsed;
}

