#include "ClockWidget.h"

#include <WiFi.h>
#include <Arduino.h>
#include <LovyanGFX.hpp>

#include "../web/Settings.h"
#include "../net/TimeService.h"

void ClockWidget::update(uint32_t now) {
  time_t nowTime;
  time(&nowTime);
  localtime_r(&nowTime, &timeinfo);
}

void ClockWidget::render(lgfx::LGFX_Sprite& ui) {
  ui.fillScreen(TFT_BLACK);
  ui.setTextColor(TFT_WHITE, TFT_BLACK);
  ui.setTextDatum(middle_center);

  int hour = timeinfo.tm_hour;
  bool pm = false;

  if (!settings.clock24h) {
    pm = hour >= 12;
    hour = hour % 12;
    if (hour == 0) hour = 12;
  }

  char buf[16];
  snprintf(
    buf,
    sizeof(buf),
    settings.clock24h ? "%02d:%02d" : "%02d:%02d %s",
    hour,
    timeinfo.tm_min,
    pm ? "PM" : "AM"
  );

  ui.setTextSize(4);
  ui.drawString(buf, ui.width() / 2, ui.height() / 2);

  ui.setTextSize(1);
  ui.setTextColor(timeSynced() ? TFT_WHITE : 0x39E7, TFT_BLACK);
  ui.drawString(
    timeSynced() ? timeServerInUse() : "waiting for NTP",
    ui.width() / 2,
    ui.height() - 12
  );
}
