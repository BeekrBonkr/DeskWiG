#pragma once
#include <Arduino.h>

// Display controls owned by main.cpp, for the web API and the knob menu.
void backlightSet(uint8_t pct);        // 1-100 percent
void displayApplyOrientation();        // from settings.displayFlip

// Deep sleep until the knob button is pressed. Waking is a cold boot.
void powerOff();
