#include "ui/ui.h"
#include <Arduino.h>
#include <cstring>
#include "core/boot.h"
#include "core/db_task.h"
#include "core/power.h"
#include "radio/ble_scanner.h"
#include "radio/lora_scanner.h"
#include "radio/wifi_sniffer.h"
#include "settings/settings.h"
#include "ui/display.h"
#include "ui/touch.h"
#include "ui/ui_internal.h"

using namespace ui_color;

namespace {
volatile Screen g_screen = Screen::TARGETS;
Target* g_snap = nullptr;  // PSRAM, kCapacity entries
size_t g_n = 0;

struct Banner { bool on; bool red; uint32_t until; SourceType type; char id[24]; char label[24]; } g_banner = {};
SemaphoreHandle_t g_banner_mx = nullptr;

// Touch gesture tracking
bool g_touch_down = false;
int16_t g_tx0, g_ty0, g_tx, g_ty;

Input read_input() {
  Input in = {};
  in.btn = buttons::poll();
  int16_t x, y;
  bool down = touch::read(x, y);
  if (down) {
    if (!g_touch_down) { g_tx0 = x; g_ty0 = y; }
    g_tx = x; g_ty = y;
    g_touch_down = true;
  } else if (g_touch_down) {
    g_touch_down = false;
    int dx = g_tx - g_tx0, dy = g_ty - g_ty0;
    if (abs(dx) > 60 && abs(dx) > 2 * abs(dy)) in.swipe = dx < 0 ? -1 : 1;
    else if (abs(dx) < 15 && abs(dy) < 15) { in.tap = true; in.x = g_tx; in.y = g_ty; }
  }
  return in;
}

void draw_status(Arduino_Canvas* c) {
  c->fillRect(0, 0, TFT_W, kStatusH, kPanel);
  c->setTextSize(1);
  c->setTextColor(kText);
  c->setCursor(4, 8);
  const Settings& s = settings::get();
  c->print(s.region == Region::FCC ? "FCC" : "EU ");
  c->setCursor(28, 8);
  if (!g_boot.lora) {
    c->setTextColor(kRed);
    c->printf("LORA x %d", g_boot.lora_err);
  } else {
    c->printf("%.14s", lora_scanner::current_plan());
  }
  c->setTextColor(g_boot.sd ? kGreen : kGrey);
  c->setCursor(128, 8);
  c->print(g_boot.sd ? "SD+" : "SD-");
  int8_t pct = power::battery_pct();
  c->setTextColor(kText);
  c->setCursor(160, 8);
  if (pct < 0) c->print("bat --");
  else c->printf("%s%3d%%", power::charging() ? "+" : " ", pct);
}

void draw_banner(Arduino_Canvas* c, uint32_t now) {
  Banner b;
  xSemaphoreTake(g_banner_mx, portMAX_DELAY);
  if (g_banner.on && (int32_t)(now - g_banner.until) >= 0) g_banner.on = false;
  b = g_banner;
  xSemaphoreGive(g_banner_mx);
  if (!b.on) return;
  c->fillRect(0, kStatusH, TFT_W, 40, b.red ? kRed : kAmber);
  c->setTextColor(BLACK);
  c->setTextSize(1);
  c->setCursor(6, kStatusH + 6);
  c->print(b.red ? "ALERT - tap to hunt" : "Possible drone");
  c->setTextSize(2);
  c->setCursor(6, kStatusH + 18);
  c->printf("%.17s", b.label[0] ? b.label : b.id);
}

bool banner_tap(const Input& in) {
  if (!in.tap || in.y < kStatusH || in.y > kStatusH + 40) return false;
  xSemaphoreTake(g_banner_mx, portMAX_DELAY);
  Banner b = g_banner;
  g_banner.on = false;
  xSemaphoreGive(g_banner_mx);
  if (!b.on) return false;
  for (size_t i = 0; i < g_n; i++) {
    if (strcmp(g_snap[i].id, b.id) == 0) {
      ui::lock_target(g_snap[i]);
      ui::go(Screen::HUNT);
      return true;
    }
  }
  return true;
}

void ui_task(void*) {
  void* mem = heap_caps_malloc(sizeof(Target) * TargetDb::kCapacity, MALLOC_CAP_SPIRAM);
  g_snap = (Target*)(mem ? mem : malloc(sizeof(Target) * 32));
  size_t cap = mem ? TargetDb::kCapacity : 32;
  Target locked;
  for (;;) {
    uint32_t now = millis();
    Arduino_Canvas* c = display::canvas();
    size_t n = db::snapshot(g_snap, cap);
    // Hide LoRa infrastructure nodes when asked to.
    if (!settings::get().show_lora_nodes) {
      size_t k = 0;
      for (size_t i = 0; i < n; i++)
        if (!is_lora_node(g_snap[i].type)) g_snap[k++] = g_snap[i];
      n = k;
    }
    g_n = n;
    Input in = read_input();
    bool consumed = banner_tap(in);

    c->fillScreen(kBg);
    switch (g_screen) {
      case Screen::TARGETS:
        if (!consumed) ui_targets::input(in, g_snap, n);
        if (g_screen == Screen::TARGETS) ui_targets::draw(c, g_snap, n);
        break;
      case Screen::HUNT:
        if (!db::locked(&locked)) { g_screen = Screen::TARGETS; break; }
        if (!consumed) ui_hunt::input(in, locked);
        if (g_screen == Screen::HUNT) ui_hunt::draw(c, locked, now);
        break;
      case Screen::SPECTRUM:
        if (!consumed) ui_spectrum::input(in);
        ui_spectrum::draw(c);
        break;
      case Screen::SETTINGS:
        if (!consumed) ui_settings::input(in);
        ui_settings::draw(c);
        break;
    }
    draw_status(c);
    draw_banner(c, now);
    display::flush();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
}  // namespace

// ---- shared helpers ----
bool is_lora_node(SourceType t) {
  return t == SourceType::MESHTASTIC || t == SourceType::LORAWAN || t == SourceType::LORA_UNKNOWN;
}
uint16_t target_color(const Target& t) {
  if (is_lora_node(t.type)) return kGrey;
  if (t.conf == Confidence::DECODED || t.type == SourceType::ELRS) return kRed;
  return kAmber;
}
char target_icon(const Target& t) {
  switch (t.type) {
    case SourceType::REMOTEID_WIFI:
    case SourceType::REMOTEID_BLE:
    case SourceType::DJI_LEGACY: return 'D';
    case SourceType::ELRS:
    case SourceType::FHSS: return 'R';
    case SourceType::DRONE_WIFI: return 'W';
    default: return 'L';
  }
}
void format_freq(const Target& t, char* out, size_t cap) {
  switch (t.type) {
    case SourceType::REMOTEID_BLE: snprintf(out, cap, "BLE"); return;
    case SourceType::REMOTEID_WIFI:
    case SourceType::DJI_LEGACY:
    case SourceType::DRONE_WIFI:
      snprintf(out, cap, "WiFi ch%u", (unsigned)((t.freq_khz - 2407000) / 5000)); return;
    default:
      snprintf(out, cap, "%u.%03u MHz", (unsigned)(t.freq_khz / 1000), (unsigned)(t.freq_khz % 1000));
  }
}
uint16_t dim(uint16_t c) { return (uint16_t)((c >> 1) & 0x7BEF); }

namespace ui {
void start() {
  g_banner_mx = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(ui_task, "ui", 8192, nullptr, 1, nullptr, 1);
}
void go(Screen s) { g_screen = s; }
Screen current() { return g_screen; }

void show_banner(const Target& t, bool red) {
  if (!g_banner_mx) return;
  xSemaphoreTake(g_banner_mx, portMAX_DELAY);
  g_banner.on = true;
  g_banner.red = red;
  g_banner.until = millis() + 5000;
  g_banner.type = t.type;
  memcpy(g_banner.id, t.id, sizeof g_banner.id);
  memcpy(g_banner.label, t.label, sizeof g_banner.label);
  xSemaphoreGive(g_banner_mx);
}

void lock_target(const Target& t) {
  unlock_target();
  if (!db::lock(t.type, t.id)) return;
  switch (t.type) {
    case SourceType::REMOTEID_WIFI:
    case SourceType::DJI_LEGACY:
    case SourceType::DRONE_WIFI:
      wifi_sniffer::pin_channel((uint8_t)((t.freq_khz - 2407000) / 5000));
      break;
    case SourceType::REMOTEID_BLE:
      ble_scanner::set_continuous(true);
      break;
    case SourceType::FHSS:
      lora_scanner::lock(t.freq_khz, 0, 0, true);
      break;
    default:  // ELRS, Meshtastic, LoRaWAN, unknown LoRa
      lora_scanner::lock(t.freq_khz, t.lora.sf, t.lora.bw_hz, false);
      break;
  }
}

void unlock_target() {
  db::unlock();
  wifi_sniffer::pin_channel(0);
  ble_scanner::set_continuous(false);
  lora_scanner::unlock();
}
}  // namespace ui
