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

  // Rotary encoder with push button (KY-040 or a bare encoder). All three
  // inputs use the internal pull-ups; the common pins go to GND.
  //   turn        -> previous / next widget
  //   click       -> show the connection info (address and API key)
  constexpr int ENC_CLK = 5;    // A
  constexpr int ENC_DT  = 6;    // B
  constexpr int ENC_SW  = 7;    // push button
  // Quadrature transitions per click of the knob: 4 for most KY-040 boards,
  // 2 for encoders that only pass one half-cycle per detent.
  constexpr int ENC_STEPS_PER_DETENT = 4;
  // Set true if clockwise goes to the previous widget (or swap CLK and DT).
  constexpr bool ENC_REVERSE = false;
}
