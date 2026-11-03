#include <Arduino.h>
#include "core/db_task.h"
#include "ui/geo.h"
#include "ui/ui.h"
#include "ui/ui_internal.h"

using namespace ui_color;

namespace {
constexpr int kMeterX = 8, kMeterY = 76, kMeterW = 46, kMeterH = 250;
constexpr int kResetX = 64, kResetY = 196, kResetW = 152, kResetH = 30;
constexpr int kGraphX = 64, kGraphY = 236, kGraphW = 152, kGraphH = 90;
constexpr float kMin = -120, kMax = -30;

int meter_y(float dbm) {
  float t = (dbm - kMin) / (kMax - kMin);
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  return kMeterY + kMeterH - (int)(t * kMeterH);
}

void draw_arrow(Arduino_Canvas* c, Trend tr, int x, int y) {
  if (tr == Trend::WARMER) {
    c->fillTriangle(x, y + 30, x + 40, y + 30, x + 20, y, kGreen);
  } else if (tr == Trend::COLDER) {
    c->fillTriangle(x, y, x + 40, y, x + 20, y + 30, kBlue);
  } else {
    c->fillRect(x, y + 12, 40, 6, kGrey);
  }
}

void line(Arduino_Canvas* c, int& y, const char* fmt, ...) {
  char buf[48];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  c->setCursor(6, y);
  c->print(buf);
  y += 11;
}
}  // namespace

namespace ui_hunt {
void draw(Arduino_Canvas* c, const Target& t, uint32_t now) {
  uint16_t col = target_color(t);
  c->setTextSize(2);
  c->setTextColor(col);
  c->setCursor(6, kStatusH + 6);
  c->printf("%.12s", t.label[0] ? t.label : t.id);
  draw_back(c);
  char f[20];
  format_freq(t, f, sizeof f);
  c->setTextSize(1);
  c->setTextColor(kGrey);
  c->setCursor(6, kStatusH + 28);
  uint32_t silent_s = (now - t.last_ms) / 1000;
  c->printf("%s  hits %lu  %lus ago", f, (unsigned long)t.hits, (unsigned long)silent_s);
  if (silent_s >= 5) {  // nothing new heard: the meter is showing the last value
    c->fillRect(0, kStatusH + 40, TFT_W, 22, kAmber);
    c->setTextColor(BLACK);
    c->setCursor(6, kStatusH + 47);
    c->printf("No signal for %lus - meter frozen", (unsigned long)silent_s);
  }

  // Meter + peak line
  c->drawRect(kMeterX, kMeterY, kMeterW, kMeterH, kDim);
  int top = meter_y(t.ema_rssi);
  c->fillRect(kMeterX + 2, top, kMeterW - 4, kMeterY + kMeterH - top - 1, col);
  int py = meter_y(t.peak_rssi);
  c->drawFastHLine(kMeterX - 4, py, kMeterW + 8, kText);

  // Smoothed dBm + trend
  c->setTextColor(kText);
  c->setTextSize(4);
  c->setCursor(kResetX, 86);
  c->printf("%d", (int)lroundf(t.ema_rssi));
  c->setTextSize(1);
  c->setCursor(kResetX + 2, 122);
  c->print("dBm (smoothed)");
  draw_arrow(c, trend_of(t, now), kResetX + 100, 84);
  c->setCursor(kResetX + 2, 140);
  c->printf("peak %d dBm  last %d", t.peak_rssi, t.last_rssi);
  if (t.snr_db) {
    c->setCursor(kResetX + 2, 154);
    c->printf("SNR %d dB", t.snr_db);
  }

  // Reset-peak button
  c->drawRoundRect(kResetX, kResetY, kResetW, kResetH, 6, kGrey);
  c->setTextSize(2);
  c->setCursor(kResetX + 16, kResetY + 8);
  c->print("RESET PEAK");

  // 30 s history graph
  c->drawRect(kGraphX, kGraphY, kGraphW, kGraphH, kDim);
  int prev_x = -1, prev_y = 0;
  for (uint8_t k = 0; k < t.hist_len; k++) {
    const Target::Sample& s = t.hist[(t.hist_head + 64 - t.hist_len + k) % 64];
    uint32_t age = now - s.t_ms;
    if (age > 30000) continue;
    int x = kGraphX + kGraphW - 1 - (int)(age * (kGraphW - 2) / 30000);
    float v = (s.ema - kMin) / (kMax - kMin);
    v = v < 0 ? 0 : (v > 1 ? 1 : v);
    int y = kGraphY + kGraphH - 2 - (int)(v * (kGraphH - 4));
    if (prev_x >= 0) c->drawLine(prev_x, prev_y, x, y, col);
    prev_x = x;
    prev_y = y;
  }

  // Detail panel
  c->setTextSize(1);
  c->setTextColor(kText);
  int y = 336;
  const OdidFields& o = t.odid;
  if (t.type == SourceType::REMOTEID_WIFI || t.type == SourceType::REMOTEID_BLE) {
    if (o.has_basic) line(c, y, "ID %s (UA type %u)", o.uas_id, o.ua_type);
    if (o.has_loc && o.loc_valid)
      line(c, y, "Drone %.5f, %.5f", o.lat, o.lon);
    else
      line(c, y, "Drone: no fix");
    if (o.has_loc && o.loc_valid)
      line(c, y, "Alt %.0fm  %.1fm/s  hdg %.0f", o.alt_geo_m, o.speed_h_ms, o.direction_deg);
    if (o.has_sys && o.op_valid)
      line(c, y, "Pilot %.5f, %.5f", o.op_lat, o.op_lon);
    else
      line(c, y, "Pilot: no fix");
    double d;
    if (drone_pilot_distance_m(o, &d)) line(c, y, "Drone-pilot %.0f m", d);
    if (o.has_opid) line(c, y, "Operator %s", o.operator_id);
    if (o.has_self) line(c, y, "\"%s\"", o.self_id);
  } else {
    if (t.lora.sf) line(c, y, "LoRa SF%u  BW %.1f kHz", t.lora.sf, t.lora.bw_hz / 1000.0f);
    if (t.lora.rate_hz) line(c, y, "Packet rate ~%u Hz", t.lora.rate_hz);
    if (t.vendor[0]) line(c, y, "Vendor %s", t.vendor);
    line(c, y, "ID %s", t.id);
  }

  c->setTextColor(kGrey);
  c->setCursor(6, 452);
  c->print("Hold to chest, turn slowly -");
  c->setCursor(6, 464);
  c->print("strongest when you face it.");
}

void input(const Input& in, const Target&) {
  if (back_pressed(in) || in.btn == Btn::UP_LONG) {
    ui::unlock_target();
    ui::go(Screen::TARGETS);
    return;
  }
  if (in.tap && in.x >= kResetX && in.x < kResetX + kResetW && in.y >= kResetY &&
      in.y < kResetY + kResetH)
    db::reset_peak();
}
}  // namespace ui_hunt
