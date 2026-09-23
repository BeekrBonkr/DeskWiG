#pragma once

// Pin assignments for the ESP32-S3-DevKitC-1 + 1.9" ST7789 build.
// Change these if you wire the display differently.
namespace Board {
  // Display (SPI2)
  constexpr int TFT_SCLK = 12;
  constexpr int TFT_MOSI = 11;
  constexpr int TFT_CS   = 10;
  constexpr int TFT_DC   = 9;
  constexpr int TFT_RST  = 8;

  // Onboard WS2812 status LED
  constexpr int STATUS_LED = 48;

  // Recovery jumper / button. Pulled up internally; bridge to GND.
  //   Held at boot            -> force setup hotspot
  //   Held for RESET_HOLD_MS  -> factory reset
  constexpr int RECOVERY_PIN = 4;
  constexpr uint32_t RESET_HOLD_MS = 8000;
}
