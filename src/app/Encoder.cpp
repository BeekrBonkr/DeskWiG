#include "Encoder.h"
#include "Board.h"
#include "Log.h"

// Quadrature decoding: index by (previous CLK, previous DT, CLK, DT) and
// get -1, 0 or +1. Invalid transitions (both lines changing at once, which
// is what bounce looks like) count as 0.
static const int8_t QDEC[16] = { 0, -1, 1, 0,  1, 0, 0, -1,  -1, 0, 0, 1,  0, 1, -1, 0 };

static volatile int32_t transitions = 0;   // raw quadrature steps, + = clockwise
static volatile uint8_t qstate = 0;

static void IRAM_ATTR onEdge() {
  uint8_t now = (digitalRead(Board::ENC_CLK) << 1) | digitalRead(Board::ENC_DT);
  qstate = ((qstate << 2) | now) & 0x0F;
  transitions += QDEC[qstate];
}

static int32_t carried = 0;          // transitions not yet turned into a detent
static bool     btnDown = false;     // debounced button state
static bool     btnRaw = false;
static uint32_t btnChangedAt = 0;
static uint32_t btnDownAt = 0;
static bool     longFired = false;

constexpr uint32_t DEBOUNCE_MS   = 25;
constexpr uint32_t LONG_PRESS_MS = 1500;

void encoderBegin() {
  pinMode(Board::ENC_CLK, INPUT_PULLUP);
  pinMode(Board::ENC_DT,  INPUT_PULLUP);
  pinMode(Board::ENC_SW,  INPUT_PULLUP);
  qstate = (digitalRead(Board::ENC_CLK) << 1) | digitalRead(Board::ENC_DT);
  attachInterrupt(digitalPinToInterrupt(Board::ENC_CLK), onEdge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(Board::ENC_DT),  onEdge, CHANGE);
  btnRaw = btnDown = digitalRead(Board::ENC_SW) == LOW;
  Log.printf("[ENC] Rotary encoder on CLK=%d DT=%d SW=%d\n", Board::ENC_CLK, Board::ENC_DT, Board::ENC_SW);
}

EncoderEvent encoderLoop(uint32_t now) {
  EncoderEvent ev = { 0, EncoderButton::NONE };

  // Rotation: take the raw count atomically, hand back whole detents.
  noInterrupts();
  int32_t t = transitions;
  transitions = 0;
  interrupts();
  carried += t;
  int32_t detents = carried / Board::ENC_STEPS_PER_DETENT;
  if (detents) {
    carried -= detents * Board::ENC_STEPS_PER_DETENT;
    if (detents > 127) detents = 127;
    if (detents < -127) detents = -127;
    ev.steps = Board::ENC_REVERSE ? -detents : detents;
  }

  // Button: debounce, then report a click on release or a long press while held.
  bool raw = digitalRead(Board::ENC_SW) == LOW;
  if (raw != btnRaw) { btnRaw = raw; btnChangedAt = now; }
  if (btnRaw != btnDown && (now - btnChangedAt) >= DEBOUNCE_MS) {
    btnDown = btnRaw;
    if (btnDown) { btnDownAt = now; longFired = false; }
    else if (!longFired) ev.button = EncoderButton::CLICK;
  }
  if (btnDown && !longFired && (now - btnDownAt) >= LONG_PRESS_MS) {
    longFired = true;
    ev.button = EncoderButton::LONG_PRESS;
  }
  return ev;
}
