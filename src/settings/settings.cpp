#include "settings/settings.h"
#include <Preferences.h>

namespace {
constexpr uint8_t kVersion = 1;
Settings g_s;

Settings defaults() {
  Settings s{};
  s.region = Region::FCC;
  s.en_rid_wifi = s.en_rid_ble = s.en_drone_wifi = s.en_elrs = s.en_fhss = true;
  s.en_mesh = s.en_lorawan = true;
  s.show_lora_nodes = true;
  s.sd_log = true;
  s.sound = true;
  s.vibe = true;
  s.alert_mode = 2;
  s.volume = 6;
  s.brightness = 12;
  return s;
}
}  // namespace

namespace settings {
void load() {
  g_s = defaults();
  Preferences p;
  if (!p.begin("neth3t", true)) return;
  if (p.getUChar("ver", 0) == kVersion && p.getBytesLength("s") == sizeof(Settings))
    p.getBytes("s", &g_s, sizeof(Settings));
  p.end();
}
void save() {
  Preferences p;
  if (!p.begin("neth3t", false)) return;
  p.putUChar("ver", kVersion);
  p.putBytes("s", &g_s, sizeof(Settings));
  p.end();
}
Settings& get() { return g_s; }
}  // namespace settings
