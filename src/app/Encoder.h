#pragma once
#include <Arduino.h>

// Rotary encoder with a push button (KY-040 style). Rotation is decoded in
// interrupts with a quadrature state table, so fast spins and contact
// bounce don't produce phantom steps; the button is polled and debounced.
//
// Call encoderBegin() once, then encoderLoop() every pass of loop() and
// act on what it returns.

enum class EncoderButton : uint8_t { NONE, CLICK, LONG_PRESS };

struct EncoderEvent {
  int8_t steps;            // detents turned since the last call: + clockwise, - counterclockwise
  EncoderButton button;
};

void encoderBegin();
EncoderEvent encoderLoop(uint32_t now);
