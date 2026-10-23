#pragma once
#include <Arduino.h>

// Display controls owned by main.cpp, for the web API and the knob menu.
void backlightSet(uint8_t pct);        // 1-100 percent
void displayApplyOrientation();        // from settings.displayFlip

// Sleep timer. Anything that should count as someone looking at the screen
// (knob input, a change made from the browser) calls displayWake(), which
// restores the backlight at once and restarts the dim and off timers.
void displayWake();
