#pragma once
// LR1121 plan rotation. Pure. Focus sweeps (Spectrum screen) take every other
// visit; the normal rotation advances only on its own visits, so every plan
// slot still sees every spreading factor / channel index.
#include <cstdint>

constexpr int kSchedSlots = 7;  // sweep sub-GHz, ELRS900, mesh, sweep 2.4, ELRS24, LoRaWAN, sweep 433
struct SchedStep { bool focus; uint8_t slot; uint32_t rot; };

class Scheduler {
 public:
  SchedStep next(bool focus_on);

 private:
  uint32_t visits_ = 0, normal_ = 0;
};
