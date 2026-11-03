#include "core/health.h"
#include <Arduino.h>
#include <esp_attr.h>
#include <esp_system.h>

namespace {
RTC_NOINIT_ATTR uint32_t g_restarts;
RTC_NOINIT_ATTR uint32_t g_magic;
constexpr uint32_t kMagic = 0x4E335448;  // "N3TH"
volatile uint32_t g_last[health::kCount] = {};
const char* const kNames[health::kCount] = {"wifi", "lora", "db"};
}  // namespace

namespace health {
void beat(Task t) { g_last[t] = millis(); }

void check() {
  if (g_magic != kMagic || esp_reset_reason() == ESP_RST_POWERON) { g_magic = kMagic; g_restarts = 0; }
  uint32_t now = millis();
  for (uint8_t i = 0; i < kCount; i++) {
    if (g_last[i] == 0) continue;  // task not started (e.g. LoRa failed to init)
    if (now - g_last[i] > kStallMs) {
      Serial.printf("HEALTH task %s stalled %lu ms, restarting\n", kNames[i], (unsigned long)(now - g_last[i]));
      g_restarts++;
      delay(50);
      esp_restart();
    }
  }
}
uint32_t restarts() { return g_magic == kMagic ? g_restarts : 0; }
}  // namespace health
