#include "button.h"
#include <Arduino.h>
#include "config.h"

// Button state variables
unsigned long lastButtonPress = 0;
unsigned long lastButtonRelease = 0;
bool buttonPressed = false;
bool buttonHandled = false;

extern float targetHumidity;

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

