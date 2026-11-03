#include <Arduino.h>
#include <cstring>
#include "radio/lora_scanner.h"
#include "settings/settings.h"
#include "ui/ui.h"
#include "ui/ui_internal.h"

using namespace ui_color;

namespace {
constexpr int kRows = 200;
constexpr int kWfY = kStatusH + 34, kGraphY = kWfY + kRows + 24, kGraphH = 110;
struct Waterfall {
  int8_t row[kRows][80];
  int16_t floor[kRows];
  uint8_t n, head, len;
  uint32_t last_t;
  lora_scanner::SweepRow latest;
};
Waterfall* g_wf = nullptr;  // 3 bands, PSRAM
uint8_t g_band = 0;

uint16_t heat(int8_t dbm, int16_t floor) {
  float t = (dbm - floor) / 40.0f;
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  // black → blue → green → yellow → red
  if (t < 0.25f) return rgb(0, 0, (uint8_t)(t * 4 * 200));
  if (t < 0.5f) return rgb(0, (uint8_t)((t - 0.25f) * 4 * 220), (uint8_t)(200 - (t - 0.25f) * 4 * 200));
  if (t < 0.75f) return rgb((uint8_t)((t - 0.5f) * 4 * 255), 220, 0);
  return rgb(255, (uint8_t)(220 - (t - 0.75f) * 4 * 220), 0);
}

void ingest(uint8_t b) {
  lora_scanner::SweepRow r;
  if (!lora_scanner::latest_sweep(b, r)) return;
  Waterfall& w = g_wf[b];
  if (r.t_ms == w.last_t) return;
  if (r.n != w.n) { w.n = r.n; w.head = 0; w.len = 0; }
  memcpy(w.row[w.head], r.dbm, r.n);
  w.floor[w.head] = r.floor_dbm;
  w.head = (uint8_t)((w.head + 1) % kRows);
  if (w.len < kRows) w.len++;
  w.last_t = r.t_ms;
  w.latest = r;
}

const char* band_name(uint8_t b) {
  if (b == 1) return "433 MHz";
  if (b == 2) return "2.4 GHz";
  return settings::get().region == Region::FCC ? "915 MHz" : "868 MHz";
}
}  // namespace

namespace ui_spectrum {
void draw(Arduino_Canvas* c) {
  if (!g_wf) {
    g_wf = (Waterfall*)heap_caps_calloc(3, sizeof(Waterfall), MALLOC_CAP_SPIRAM);
    if (!g_wf) return;
  }
  for (uint8_t b = 0; b < 3; b++) ingest(b);
  Waterfall& w = g_wf[g_band];
  c->setTextSize(2);
  c->setTextColor(kText);
  c->setCursor(6, kStatusH + 8);
  c->printf("Spectrum %s", band_name(g_band));
  if (w.len == 0) {
    c->setTextSize(1);
    c->setTextColor(kGrey);
    c->setCursor(6, kWfY + 20);
    c->print("Waiting for sweep... (up/down: band)");
    return;
  }
  int cw = TFT_W / w.n;
  if (cw < 1) cw = 1;
  int x0 = (TFT_W - cw * w.n) / 2;
  for (int k = 0; k < w.len; k++) {
    int ri = (w.head + kRows - 1 - k) % kRows;  // newest on top
    for (int i = 0; i < w.n; i++) c->fillRect(x0 + i * cw, kWfY + k, cw, 1, heat(w.row[ri][i], w.floor[ri]));
  }
  // CAD hits overlay for this band
  const lora_scanner::SweepRow& r = w.latest;
  uint32_t f1 = r.f0_khz + r.step_khz * (r.n - 1);
  CadHit hits[16];
  size_t nh = lora_scanner::recent_cad(hits, 16);
  c->setTextSize(1);
  for (size_t i = 0; i < nh; i++) {
    if (hits[i].freq_khz < r.f0_khz || hits[i].freq_khz > f1 || f1 == r.f0_khz) continue;
    int x = x0 + (int)((uint64_t)(hits[i].freq_khz - r.f0_khz) * (cw * w.n - 1) / (f1 - r.f0_khz));
    c->drawFastVLine(x, kWfY + kRows + 2, 8, kText);
    c->setTextColor(kText);
    c->setCursor(constrain(x - 8, 0, TFT_W - 24), kWfY + kRows + 12);
    c->printf("SF%u", hits[i].sf);
  }
  // Latest sweep line graph, -127..-30 dBm, plus floor
  c->drawRect(0, kGraphY, TFT_W, kGraphH, kDim);
  auto gy = [](int dbm) { return kGraphY + kGraphH - 2 - (int)((dbm + 127) * (kGraphH - 4) / 97); };
  c->drawFastHLine(1, constrain(gy(r.floor_dbm), kGraphY, kGraphY + kGraphH - 1), TFT_W - 2, kBlue);
  for (int i = 1; i < r.n; i++)
    c->drawLine(x0 + (i - 1) * cw + cw / 2, gy(r.dbm[i - 1]), x0 + i * cw + cw / 2, gy(r.dbm[i]), kAmber);
  c->setTextColor(kGrey);
  c->setCursor(2, kGraphY + kGraphH + 4);
  c->printf("%u.%u", (unsigned)(r.f0_khz / 1000), (unsigned)(r.f0_khz % 1000 / 100));
  c->setCursor(TFT_W - 48, kGraphY + kGraphH + 4);
  c->printf("%u.%u", (unsigned)(f1 / 1000), (unsigned)(f1 % 1000 / 100));
  c->setCursor(76, kGraphY + kGraphH + 4);
  c->printf("floor %d dBm", r.floor_dbm);
}

void input(const Input& in) {
  if (in.btn == Btn::BACK || in.swipe == 1) { ui::go(Screen::TARGETS); return; }
  if (in.btn == Btn::UP) g_band = (uint8_t)((g_band + 2) % 3);
  if (in.btn == Btn::DOWN || in.tap) g_band = (uint8_t)((g_band + 1) % 3);
}
}  // namespace ui_spectrum
