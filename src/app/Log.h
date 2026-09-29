#pragma once
#include <Arduino.h>
#include <Print.h>

// Everything the firmware prints goes to the USB serial port and into a
// ring buffer in PSRAM, so /terminal can show the same output in the
// browser. ESP-IDF component logs (WiFi, TCP/IP...) are captured too.
//
// Writes can come from any task (the data source and image fetchers run
// on their own), so the buffer is guarded by a spinlock.
class LogBuffer : public Print {
public:
  static constexpr size_t CAPACITY = 32 * 1024;

  void begin(unsigned long baud);

  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buf, size_t n) override;

  // Copies bytes written after byte number `since` into out, at most max.
  // If `since` is older than what the buffer still holds, copying starts
  // at the oldest byte kept. *next receives the byte number to pass on
  // the following call. Returns the number of bytes copied.
  size_t read(uint32_t since, char* out, size_t max, uint32_t* next);

  // Total bytes ever written; the "since" value that means "only new output".
  uint32_t seq();

private:
  void store(const uint8_t* buf, size_t n);

  char* ring = nullptr;
  uint32_t total = 0;
};

extern LogBuffer Log;
