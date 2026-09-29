#include "Log.h"

#include <esp_log.h>
#include <esp_heap_caps.h>

LogBuffer Log;

static portMUX_TYPE logMux = portMUX_INITIALIZER_UNLOCKED;

// ESP-IDF logs (esp_log_write / ESP_LOGx) arrive here once the hook is
// installed. Formatted into a stack buffer, then treated like our own output.
static int idfLogHook(const char* fmt, va_list args) {
  char line[256];
  int n = vsnprintf(line, sizeof(line), fmt, args);
  if (n <= 0) return 0;
  if ((size_t)n >= sizeof(line)) n = sizeof(line) - 1;
  return Log.write((const uint8_t*)line, n);
}

void LogBuffer::begin(unsigned long baud) {
  Serial.begin(baud);
  if (!ring) ring = (char*)heap_caps_malloc(CAPACITY, MALLOC_CAP_SPIRAM);
  if (!ring) ring = (char*)malloc(CAPACITY);
  esp_log_set_vprintf(idfLogHook);
}

void LogBuffer::store(const uint8_t* buf, size_t n) {
  if (!ring) return;
  // Only the last CAPACITY bytes of a huge write can matter.
  if (n > CAPACITY) { buf += n - CAPACITY; n = CAPACITY; }
  portENTER_CRITICAL(&logMux);
  size_t pos = total % CAPACITY;
  size_t first = CAPACITY - pos;
  if (first > n) first = n;
  memcpy(ring + pos, buf, first);
  if (n > first) memcpy(ring, buf + first, n - first);
  total += n;
  portEXIT_CRITICAL(&logMux);
}

size_t LogBuffer::write(uint8_t c) {
  return write(&c, 1);
}

size_t LogBuffer::write(const uint8_t* buf, size_t n) {
  Serial.write(buf, n);
  store(buf, n);
  return n;
}

uint32_t LogBuffer::seq() {
  portENTER_CRITICAL(&logMux);
  uint32_t t = total;
  portEXIT_CRITICAL(&logMux);
  return t;
}

size_t LogBuffer::read(uint32_t since, char* out, size_t max, uint32_t* next) {
  if (!ring) { *next = 0; return 0; }
  portENTER_CRITICAL(&logMux);
  uint32_t oldest = total > CAPACITY ? total - CAPACITY : 0;
  if (since < oldest || since > total) since = oldest;
  size_t n = total - since;
  if (n > max) n = max;
  size_t pos = since % CAPACITY;
  size_t first = CAPACITY - pos;
  if (first > n) first = n;
  memcpy(out, ring + pos, first);
  if (n > first) memcpy(out + first, ring, n - first);
  *next = since + n;
  portEXIT_CRITICAL(&logMux);
  return n;
}
