#pragma once
#include <Arduino.h>
#include <hal/spi_types.h>   // SPI2_HOST / SPI3_HOST

// Pin assignments, one block per supported board. The PlatformIO env sets
// DESKWIG_BOARD_XIAO or DESKWIG_BOARD_DEVKITC; change a block if you wire
// the display differently.
namespace Board {

#if defined(DESKWIG_BOARD_XIAO)
  // Seeed XIAO ESP32S3 (8 MB flash, 8 MB PSRAM) + 1.9" ST7789. The board
  // exposes eleven GPIOs, D0 to D10, and this build uses every one.
  constexpr const char* NAME = "xiao-esp32s3";

  // Display. SPI3 rather than SPI2: for the default host LovyanGFX starts
  // the bus through Arduino's SPI class, which claims the variant's MISO
  // pin (GPIO 8, D9) as an input even when no MISO is given, and there is
  // no pin to spare. On the S3 every pin goes through the GPIO matrix on
  // either host, so nothing is lost.
  constexpr spi_host_device_t TFT_SPI_HOST = SPI3_HOST;
  constexpr int TFT_SCLK = 7;    // D8
  constexpr int TFT_MOSI = 9;    // D10
  constexpr int TFT_CS   = 8;    // D9
  constexpr int TFT_DC   = 44;   // D7
  constexpr int TFT_RST  = 43;   // D6
  constexpr int TFT_BL   = 6;    // D5, PWM dimmed

  // No WS2812 on the board. Wire one to D2 for the colored status light,
  // or leave it: the onboard yellow LED (GPIO 21, active low) mirrors the
  // pattern's brightness so the device still shows its state.
  constexpr int STATUS_LED  = 3;    // D2 (a strapping pin, but a WS2812 does not drive it at boot)
  constexpr int ONBOARD_LED = 21;

  // Recovery jumper / button. Pulled up internally; bridge to GND.
  constexpr int RECOVERY_PIN = 5;   // D4

  // Rotary encoder with push button.
  constexpr int ENC_CLK = 1;    // D0, A
  constexpr int ENC_DT  = 2;    // D1, B
  constexpr int ENC_SW  = 4;    // D3, push button

#else
  // ESP32-S3-DevKitC-1 (N16R8) + 1.9" ST7789.
  constexpr const char* NAME = "esp32-s3-devkitc-1";

  // Display (SPI2)
  constexpr spi_host_device_t TFT_SPI_HOST = SPI2_HOST;
  constexpr int TFT_SCLK = 12;
  constexpr int TFT_MOSI = 11;
  constexpr int TFT_CS   = 10;
  constexpr int TFT_DC   = 9;
  constexpr int TFT_RST  = 8;
  // Backlight, PWM dimmed. Most 1.9" modules switch BL through a
  // transistor, so the pin drives it directly. Not 13: with no MISO pin
  // given, the Arduino SPI layer claims the S3's default MISO (GPIO 13)
  // as an input when the bus starts, which undoes the PWM attach.
  constexpr int TFT_BL   = 14;

  // Onboard WS2812 status LED; no plain LED to mirror it on.
  constexpr int STATUS_LED  = 48;
  constexpr int ONBOARD_LED = -1;

  // Recovery jumper / button. Pulled up internally; bridge to GND.
  constexpr int RECOVERY_PIN = 4;

  // Rotary encoder with push button (KY-040 or a bare encoder). All three
  // inputs use the internal pull-ups; the common pins go to GND.
  constexpr int ENC_CLK = 5;    // A
  constexpr int ENC_DT  = 6;    // B
  constexpr int ENC_SW  = 7;    // push button
#endif

  // Recovery jumper:
  //   Held at boot            -> force setup hotspot
  //   Held for RESET_HOLD_MS  -> factory reset
  constexpr uint32_t RESET_HOLD_MS = 8000;

  // Encoder:
  //   turn        -> previous / next widget
  //   click       -> show the connection info (address and API key)
  // Quadrature transitions per click of the knob: 4 for most KY-040 boards,
  // 2 for encoders that only pass one half-cycle per detent.
  constexpr int ENC_STEPS_PER_DETENT = 4;
  // Set true if clockwise goes to the previous widget (or swap CLK and DT).
  constexpr bool ENC_REVERSE = false;

  // LEDC channel for the onboard LED. The backlight has channel 7, and
  // analogWrite() would hand out that same channel first.
  constexpr int ONBOARD_LED_PWM_CHANNEL = 6;
}
