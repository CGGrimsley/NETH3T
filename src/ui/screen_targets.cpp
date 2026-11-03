#include <Arduino.h>
#include "ui/ui.h"
#include "ui/ui_internal.h"

using namespace ui_color;

namespace {
int g_sel = 0, g_top = 0;
constexpr int kRowH = 44, kListY = kStatusH + 2;
constexpr int kVisible = (TFT_H - kListY) / kRowH;

void clamp(size_t n) {
  if (n == 0) { g_sel = g_top = 0; return; }
  if (g_sel >= (int)n) g_sel = (int)n - 1;
  if (g_sel < 0) g_sel = 0;
  if (g_sel < g_top) g_top = g_sel;
  if (g_sel >= g_top + kVisible) g_top = g_sel - kVisible + 1;
}

void open(const Target& t) {
  ui::lock_target(t);
  ui::go(Screen::HUNT);
}
}  // namespace

namespace ui_targets {
void draw(Arduino_Canvas* c, const Target* t, size_t n) {
  clamp(n);
  if (n == 0) {
    c->setTextColor(kGrey);
    c->setTextSize(2);
    c->setCursor(30, 200);
    c->print("Scanning...");
    c->setTextSize(1);
    c->setCursor(22, 228);
    c->print("No drones, RC links or LoRa yet");
    return;
  }
  uint32_t now = millis();
  for (int r = 0; r < kVisible && g_top + r < (int)n; r++) {
    const Target& x = t[g_top + r];
    int y = kListY + r * kRowH;
    uint16_t col = target_color(x);
    if (x.stale) col = dim(col);
    if (g_top + r == g_sel) c->fillRect(0, y, TFT_W, kRowH - 2, kPanel);
    c->fillRoundRect(4, y + 6, 26, 26, 4, col);
    c->setTextColor(BLACK);
    c->setTextSize(2);
    c->setCursor(11, y + 12);
    c->print(target_icon(x));
    c->setTextColor(x.stale ? dim(kText) : kText);
    c->setCursor(36, y + 4);
    c->printf("%.15s", x.label[0] ? x.label : x.id);
    char f[20];
    format_freq(x, f, sizeof f);
    c->setTextSize(1);
    c->setTextColor(x.stale ? dim(kGrey) : kGrey);
    c->setCursor(36, y + 26);
    c->printf("%s  %lus", f, (unsigned long)((now - x.last_ms) / 1000));
    // RSSI bar: -120..-30 dBm
    int w = constrain((int)((x.ema_rssi + 120) * 50 / 90), 0, 50);
    c->drawRect(166, y + 26, 52, 8, kDim);
    c->fillRect(167, y + 27, w, 6, col);
  }
}

void input(const Input& in, const Target* t, size_t n) {
  if (in.btn == Btn::UP) g_sel--;
  if (in.btn == Btn::DOWN) g_sel++;
  clamp(n);
  if (in.btn == Btn::DOWN_LONG) { ui::go(Screen::SETTINGS); return; }
  if (in.swipe == -1) { ui::go(Screen::SPECTRUM); return; }
  if (n == 0) return;
  if (in.btn == Btn::UP_LONG) { open(t[g_sel]); return; }
  if (in.tap && in.y >= kListY) {
    int idx = g_top + (in.y - kListY) / kRowH;
    if (idx < (int)n) { g_sel = idx; open(t[idx]); }
  }
}
}  // namespace ui_targets
