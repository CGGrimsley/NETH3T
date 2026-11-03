#include "alert/vibe.h"
#include <Arduino.h>
#include "board_pins.h"

namespace {
constexpr uint8_t kCh = 3;
volatile uint16_t g_once_ms = 0;
volatile bool g_double = false;
volatile uint16_t g_rate_x10 = 0;

void burst(uint16_t ms) {
  ledcWrite(kCh, 255);
  vTaskDelay(pdMS_TO_TICKS(ms));
  ledcWrite(kCh, 0);
}

void task(void*) {
  for (;;) {
    if (g_double) {
      g_double = false;
      burst(120);
      vTaskDelay(pdMS_TO_TICKS(100));
      burst(120);
    } else if (g_once_ms) {
      uint16_t ms = g_once_ms;
      g_once_ms = 0;
      burst(ms);
    } else if (uint16_t rate = g_rate_x10) {  // read once: stop() may race
      uint32_t period = 10000 / rate;
      burst(40);
      vTaskDelay(pdMS_TO_TICKS(period > 40 ? period - 40 : 1));
    } else {
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
}
}  // namespace

namespace vibe {
void init() {
  ledcSetup(kCh, 12000, 8);
  ledcAttachPin(PIN_VIBE, kCh);
  ledcWrite(kCh, 0);
  xTaskCreatePinnedToCore(task, "vibe", 2048, nullptr, 1, nullptr, 0);
}
void pulse(uint16_t ms) { g_once_ms = ms; }
void double_pulse() { g_double = true; }
void hunt_pulse(uint16_t r) { g_rate_x10 = r; }
void stop() { g_rate_x10 = 0; }
}  // namespace vibe
