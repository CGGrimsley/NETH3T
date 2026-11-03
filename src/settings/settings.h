#pragma once
#include <cstdint>
#include "detect/channel_plans.h"

struct Settings {
  Region region;
  bool en_rid_wifi, en_rid_ble, en_drone_wifi, en_elrs, en_fhss, en_mesh, en_lorawan;
  bool show_lora_nodes, sd_log, sound, vibe;
  uint8_t alert_mode;  // 0 off, 1 banner, 2 banner + sound + vibration
  uint8_t volume;      // 0-10
  uint8_t brightness;  // 1-16
};

namespace settings {
void load();  // from NVS namespace "neth3t"; defaults if missing or wrong version
void save();
Settings& get();
}  // namespace settings
