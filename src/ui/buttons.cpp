#include "ui/buttons.h"
#include <Arduino.h>
#include "board_pins.h"

namespace {
struct Key {
  uint8_t pin;
  Btn short_ev, long_ev;
  bool down;
  bool long_sent;
  uint32_t since;
};
Key g_keys[3] = {
    {PIN_BTN_BOOT, Btn::BACK, Btn::NONE, false, false, 0},
    {PIN_BTN_UP, Btn::UP, Btn::UP_LONG, false, false, 0},
    {PIN_BTN_DOWN, Btn::DOWN, Btn::DOWN_LONG, false, false, 0},
};
constexpr uint32_t kDebounceMs = 30, kLongMs = 700;
}  // namespace

namespace buttons {
void init() {
  for (Key& k : g_keys) pinMode(k.pin, INPUT_PULLUP);
}

Btn poll() {
  uint32_t now = millis();
  for (Key& k : g_keys) {
    bool pressed = digitalRead(k.pin) == LOW;
    if (pressed && !k.down) {
      k.down = true;
      k.long_sent = false;
      k.since = now;
    } else if (pressed && k.down) {
      if (!k.long_sent && k.long_ev != Btn::NONE && now - k.since >= kLongMs) {
        k.long_sent = true;
        return k.long_ev;
      }
    } else if (!pressed && k.down) {
      k.down = false;
      if (!k.long_sent && now - k.since >= kDebounceMs) return k.short_ev;
    }
  }
  return Btn::NONE;
}
}  // namespace buttons
