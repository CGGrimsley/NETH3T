#include "core/power.h"
#include <Arduino.h>
#include <Wire.h>
#define XPOWERS_CHIP_SY6970
#include <XPowersLib.h>
#include "board_pins.h"

namespace {
PowersSY6970 g_pmu;
bool g_ok = false;
}

namespace power {
bool init() {
  g_ok = g_pmu.init(Wire, PIN_I2C_SDA, PIN_I2C_SCL, SY6970_ADDR);
  if (g_ok) g_pmu.enableMeasure();
  return g_ok;
}
int8_t battery_pct() {
  if (!g_ok) return -1;
  uint16_t mv = g_pmu.getBattVoltage();
  if (mv < 2500) return -1;  // no battery
  int pct = (int)(mv - 3300) * 100 / (4200 - 3300);
  return (int8_t)constrain(pct, 0, 100);
}
bool charging() { return g_ok && g_pmu.isCharging(); }
}  // namespace power
