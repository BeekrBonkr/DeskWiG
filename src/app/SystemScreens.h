#pragma once
#include <LovyanGFX.hpp>
#include "../net/WifiManager.h"

// Firmware-owned screens: boot, setup hotspot, connection info, recovery.
// All share the same template: size-2 title at the top, size-1 body,
// one dim line at the bottom.

void drawApScreen(lgfx::LGFX_Sprite& ui, ApReason reason, const char* connectingTo);
void drawConnectingScreen(lgfx::LGFX_Sprite& ui, const char* ssid);
void drawInfoScreen(lgfx::LGFX_Sprite& ui, uint32_t secondsLeft);
void drawRecoveryScreen(lgfx::LGFX_Sprite& ui, uint32_t msUntilReset);
void drawResettingScreen(lgfx::LGFX_Sprite& ui);
