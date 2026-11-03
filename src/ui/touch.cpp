#include "ui/touch.h"
#include <Arduino.h>
#include <Wire.h>
#include <TouchDrvCSTXXX.hpp>
#include "board_pins.h"

namespace {
TouchDrvCSTXXX g_touch;
bool g_ok = false;
constexpr uint8_t kCst226Addr = 0x5A;
}

namespace touch {
bool init() {
  g_touch.setPins(PIN_TOUCH_RST, PIN_TOUCH_INT);
  g_ok = g_touch.begin(Wire, kCst226Addr, PIN_I2C_SDA, PIN_I2C_SCL);
  if (g_ok) g_touch.setMaxCoordinates(TFT_W, TFT_H);
  return g_ok;
}
bool read(int16_t& x, int16_t& y) {
  if (!g_ok) return false;
  const TouchPoints& pts = g_touch.getTouchPoints();
  if (!pts.hasPoints()) return false;
  const TouchPoint& p = pts.getPoint(0);
  x = (int16_t)p.x;
  y = (int16_t)p.y;
  return x >= 0 && x < TFT_W && y >= 0 && y < TFT_H;
}
}  // namespace touch
