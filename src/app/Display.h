#pragma once
#include <Arduino.h>

// Display controls owned by main.cpp, for the web API and the knob menu.
void backlightSet(uint8_t pct);        // 1-100 percent
void displayApplyOrientation();        // from settings.displayFlip
