#include "FetchLock.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SemaphoreHandle_t fetchMutex() {
  static SemaphoreHandle_t m = xSemaphoreCreateMutex();
  return m;
}

FetchGuard::FetchGuard()  { if (fetchMutex()) xSemaphoreTake(fetchMutex(), portMAX_DELAY); }
FetchGuard::~FetchGuard() { if (fetchMutex()) xSemaphoreGive(fetchMutex()); }
