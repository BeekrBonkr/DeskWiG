#pragma once
#include <LovyanGFX.hpp>
#include "Encoder.h"

// Settings menu driven by the rotary encoder: hold the button to open it,
// turn to move, click to open or set, hold to go back. It covers what the
// setup page offers that makes sense without a keyboard: brightness, the
// status LED, clock and timezone, joining or forgetting WiFi, the device
// name, and the connection details (address and API key).
//
// While the menu is open main.cpp hands every encoder event to menuInput()
// and draws menuRender() instead of the active widget.

void menuOpen(uint32_t now);
void menuClose();
bool menuActive();
void menuInput(const EncoderEvent& ev, uint32_t now);
void menuRender(lgfx::LGFX_Sprite& ui, uint32_t now);
