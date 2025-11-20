#ifndef DEHUMIDIFIER_H
#define DEHUMIDIFIER_H

#include "config.h"

// Global dehumidifier variables (extern declarations)
extern bool dehumidifierRunning;
extern bool floatSwitchTriggered;
extern bool compressorInCooldown;
extern unsigned long lastCompressorOff;
extern float targetHumidity;
extern float currentHumidity;

// Function declarations
void checkFloatSwitch();
void controlDehumidifier();
unsigned long getCooldownRemaining();

#endif // DEHUMIDIFIER_H

