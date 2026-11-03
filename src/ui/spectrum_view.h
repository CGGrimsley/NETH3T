#pragma once
// Pure geometry for the landscape spectrum analyzer: zoom, axis mapping,
// peak hold, and touch rotation.
#include <cstdint>
#include "detect/channel_plans.h"

struct Span { uint32_t f0_khz, f1_khz; };
struct BandLimits { uint32_t lo_khz, hi_khz, min_span_khz; };

// 0: 915/868 region band, 1: 433 MHz, 2: 2.4 GHz
BandLimits band_limits(uint8_t band, Region r);
Span full_span(const BandLimits& b);
Span zoom_in(Span s, uint32_t center_khz, const BandLimits& b);   // halves span, clamps to band
Span zoom_out(Span s, uint32_t center_khz, const BandLimits& b);  // doubles span, clamps to band
int freq_to_x(uint32_t f_khz, Span s, int width);                 // clamped to 0..width-1
uint32_t x_to_freq(int x, Span s, int width);

// Per-bin maximum, decaying 1 dB per second.
class PeakHold {
 public:
  static constexpr uint8_t kMax = 80;
  void reset() { n_ = 0; }
  void update(const int8_t* dbm, uint8_t n, uint32_t now_ms);  // resets if n changes
  int8_t at(uint8_t i) const;
  uint8_t n() const { return n_; }

 private:
  float peak_[kMax] = {};
  uint8_t n_ = 0;
  uint32_t last_ms_ = 0;
};

// True if a sweep row (first/last frequency, point count) came from `want`.
bool sweep_matches(uint32_t f0_khz, uint32_t f_last_khz, uint8_t n, const ScanPlan& want);
// Rounds to the nearest multiple of `grid_khz` (used to keep zoomed hot bins on
// the normal 200 kHz grid for the FHSS detector).
uint32_t snap_khz(uint32_t f_khz, uint32_t grid_khz);

// Portrait touch (222x480) to landscape canvas coordinates (480x222) for GFX rotation 1 or 3.
void touch_to_landscape(int16_t px, int16_t py, uint8_t rotation, int16_t* lx, int16_t* ly);
