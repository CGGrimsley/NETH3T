#pragma once
// Per-band noise floor from RSSI sweeps: mean of the 20th-percentile bin value
// over the last 16 sweeps. A bin is "hot" when > floor + 10 dB. Pure.
#include <cstddef>
#include <cstdint>

class NoiseFloor {
 public:
  static constexpr uint8_t kHistory = 16;
  static constexpr int8_t kHotDb = 10;
  void add_sweep(const int8_t* dbm, uint8_t n);
  bool ready() const { return n_ > 0; }
  int16_t floor_dbm() const;      // -127 when not ready
  bool hot(int8_t dbm) const;     // false until ready

 private:
  int8_t p20_[kHistory] = {};
  uint8_t n_ = 0, head_ = 0;
};
