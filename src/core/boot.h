#pragma once
#include <cstdint>
// Result of each bring-up step; shown on the boot screen and in the status bar.
struct BootStatus {
  bool pmu, display, touch, lora, sd, rtc;
  int16_t lora_err;
};
extern BootStatus g_boot;
