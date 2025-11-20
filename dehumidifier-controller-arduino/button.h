#ifndef BUTTON_H
#define BUTTON_H

#include "config.h"

// Global button variables (extern declarations)
extern unsigned long lastButtonPress;
extern unsigned long lastButtonRelease;
extern bool buttonPressed;
extern bool buttonHandled;
extern float targetHumidity;

// Function declarations
void handleButton();

#endif // BUTTON_H

