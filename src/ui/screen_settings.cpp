#include <Arduino.h>
#include "core/db_task.h"
#include "core/health.h"
#include "radio/lora_scanner.h"
#include "settings/settings.h"
#include "ui/display.h"
#include "ui/ui.h"
#include "ui/ui_internal.h"

using namespace ui_color;

namespace {
enum Item : uint8_t {
  REGION, RID_WIFI, RID_BLE, DRONE_WIFI, ELRS, FHSS, MESH, LORAWAN, SHOW_NODES, SD_LOG,
  SOUND, VIBE, ALERT_MODE, VOLUME, BRIGHTNESS, kItems
};
const char* const kLabels[kItems] = {
    "Region",     "RemoteID WiFi", "RemoteID BLE", "Drone WiFi", "ELRS",     "FHSS",
    "Meshtastic", "LoRaWAN",       "Show LoRa",    "SD log",     "Sound",    "Vibration",
    "Alerts",     "Volume",        "Brightness",
};
constexpr int kRowH = 24, kListY = kStatusH + 40, kAboutRows = 6;
constexpr int kVisible = (TFT_H - kListY - kAboutRows * 12 - 8) / kRowH;
int g_sel = 0, g_top = 0;

bool* flag(Settings& s, uint8_t i) {
  switch (i) {
    case RID_WIFI: return &s.en_rid_wifi;
    case RID_BLE: return &s.en_rid_ble;
    case DRONE_WIFI: return &s.en_drone_wifi;
    case ELRS: return &s.en_elrs;
    case FHSS: return &s.en_fhss;
    case MESH: return &s.en_mesh;
    case LORAWAN: return &s.en_lorawan;
    case SHOW_NODES: return &s.show_lora_nodes;
    case SD_LOG: return &s.sd_log;
    case SOUND: return &s.sound;
    case VIBE: return &s.vibe;
    default: return nullptr;
  }
}

void value(const Settings& s, uint8_t i, char* out, size_t cap) {
  const bool* f = flag(const_cast<Settings&>(s), i);
  if (f) { snprintf(out, cap, "%s", *f ? "ON" : "off"); return; }
  switch (i) {
    case REGION: snprintf(out, cap, "%s", s.region == Region::FCC ? "FCC" : "EU"); break;
    case ALERT_MODE: snprintf(out, cap, "%s", s.alert_mode == 0 ? "off" : (s.alert_mode == 1 ? "banner" : "full")); break;
    case VOLUME: snprintf(out, cap, "%u", s.volume); break;
    case BRIGHTNESS: snprintf(out, cap, "%u", s.brightness); break;
  }
}

void activate(uint8_t i) {
  Settings& s = settings::get();
  if (bool* f = flag(s, i)) {
    *f = !*f;
  } else {
    switch (i) {
      case REGION:
        s.region = s.region == Region::FCC ? Region::EU : Region::FCC;
        lora_scanner::set_region(s.region);
        break;
      case ALERT_MODE: s.alert_mode = (uint8_t)((s.alert_mode + 1) % 3); break;
      case VOLUME: s.volume = (uint8_t)((s.volume + 1) % 11); break;
      case BRIGHTNESS:
        s.brightness = (uint8_t)(s.brightness % 16 + 1);
        display::set_brightness(s.brightness);
        break;
    }
  }
  settings::save();
}

void clamp() {
  if (g_sel < 0) g_sel = kItems - 1;
  if (g_sel >= kItems) g_sel = 0;
  if (g_sel < g_top) g_top = g_sel;
  if (g_sel >= g_top + kVisible) g_top = g_sel - kVisible + 1;
}
}  // namespace

namespace ui_settings {
void draw(Arduino_Canvas* c) {
  clamp();
  const Settings& s = settings::get();
  c->setTextSize(2);
  c->setTextColor(kText);
  c->setCursor(6, kStatusH + 6);
  c->print("Settings");
  draw_back(c);
  for (int r = 0; r < kVisible && g_top + r < kItems; r++) {
    uint8_t i = (uint8_t)(g_top + r);
    int y = kListY + r * kRowH;
    if (i == g_sel) c->fillRect(0, y - 3, TFT_W, kRowH - 2, kPanel);
    c->setTextColor(kText);
    c->setCursor(6, y);
    c->print(kLabels[i]);
    char v[12];
    value(s, i, v, sizeof v);
    c->setTextColor(kAmber);
    c->setCursor(TFT_W - 6 - 12 * (int)strlen(v), y);
    c->print(v);
  }
  // About / debug
  lora_scanner::Stats st = lora_scanner::stats();
  int y = TFT_H - kAboutRows * 12 - 4;
  c->setTextSize(1);
  c->setTextColor(kGrey);
  c->setCursor(6, y); c->printf("%s", lora_scanner::version()); y += 12;
  c->setCursor(6, y); c->printf("CAD hits %lu  RX pkts %lu", (unsigned long)st.cad_hits, (unsigned long)st.rx_pkts); y += 12;
  c->setCursor(6, y); c->printf("Radio errors %lu  suspended %lu", (unsigned long)st.errors, (unsigned long)st.suspended); y += 12;
  c->setCursor(6, y); c->printf("Dropped obs %lu  restarts %lu", (unsigned long)db::dropped(), (unsigned long)health::restarts()); y += 12;
  c->setCursor(6, y); c->printf("Free heap %lu  PSRAM %lu", (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram()); y += 12;
  c->setCursor(6, y); c->print("Up/down select, long-press up: change");
}

void input(const Input& in) {
  if (back_pressed(in)) { ui::go(Screen::TARGETS); return; }
  if (in.btn == Btn::UP) g_sel--;
  if (in.btn == Btn::DOWN) g_sel++;
  clamp();
  if (in.btn == Btn::UP_LONG) activate((uint8_t)g_sel);
  if (in.tap && in.y >= kListY - 3) {
    int r = (in.y - kListY + 3) / kRowH;
    if (r < kVisible && g_top + r < kItems) {
      g_sel = g_top + r;
      activate((uint8_t)g_sel);
    }
  }
}
}  // namespace ui_settings
