#include "ui/spectrum_view.h"
#include <cmath>

BandLimits band_limits(uint8_t band, Region r) {
  switch (band) {
    case 1: return {433050, 434790, 4000};
    case 2: return {2400000, 2480000, 16000};
    default: return r == Region::FCC ? BandLimits{902000, 928000, 4000} : BandLimits{863000, 870000, 4000};
  }
}

Span full_span(const BandLimits& b) { return {b.lo_khz, b.hi_khz}; }

namespace {
Span centred(uint32_t span, uint32_t center, const BandLimits& b) {
  uint32_t full = b.hi_khz - b.lo_khz;
  if (span >= full) return full_span(b);
  int64_t f0 = (int64_t)center - span / 2;
  if (f0 < b.lo_khz) f0 = b.lo_khz;
  if (f0 + span > b.hi_khz) f0 = b.hi_khz - span;
  return {(uint32_t)f0, (uint32_t)f0 + span};
}
}  // namespace

Span zoom_in(Span s, uint32_t center, const BandLimits& b) {
  uint32_t span = (s.f1_khz - s.f0_khz) / 2;
  if (span < b.min_span_khz) span = b.min_span_khz;
  return centred(span, center, b);
}

Span zoom_out(Span s, uint32_t center, const BandLimits& b) {
  return centred((s.f1_khz - s.f0_khz) * 2, center, b);
}

int freq_to_x(uint32_t f, Span s, int width) {
  if (f <= s.f0_khz || s.f1_khz <= s.f0_khz) return 0;
  if (f >= s.f1_khz) return width - 1;
  return (int)((uint64_t)(f - s.f0_khz) * (width - 1) / (s.f1_khz - s.f0_khz));
}

uint32_t x_to_freq(int x, Span s, int width) {
  if (x <= 0 || width < 2) return s.f0_khz;
  if (x >= width - 1) return s.f1_khz;
  return s.f0_khz + (uint32_t)((uint64_t)x * (s.f1_khz - s.f0_khz) / (width - 1));
}

void PeakHold::update(const int8_t* dbm, uint8_t n, uint32_t now_ms) {
  if (n > kMax) n = kMax;
  if (n != n_) {
    n_ = n;
    for (uint8_t i = 0; i < n; i++) peak_[i] = dbm[i];
    last_ms_ = now_ms;
    return;
  }
  float decay = (now_ms - last_ms_) / 1000.0f;  // 1 dB per second
  last_ms_ = now_ms;
  for (uint8_t i = 0; i < n; i++) {
    float p = peak_[i] - decay;
    peak_[i] = dbm[i] > p ? dbm[i] : p;
  }
}

int8_t PeakHold::at(uint8_t i) const {
  if (i >= n_) return -127;
  long v = lroundf(peak_[i]);
  return (int8_t)(v < -127 ? -127 : (v > 0 ? 0 : v));
}

bool sweep_matches(uint32_t f0, uint32_t f_last, uint8_t n, const ScanPlan& want) {
  return n == want.n_freqs && n > 0 && f0 == want.freqs_khz[0] && f_last == want.freqs_khz[n - 1];
}

uint32_t snap_khz(uint32_t f, uint32_t grid) { return grid ? (f + grid / 2) / grid * grid : f; }

void touch_to_landscape(int16_t px, int16_t py, uint8_t rotation, int16_t* lx, int16_t* ly) {
  // Inverse of Adafruit-GFX rotation on a 222x480 portrait panel.
  if (rotation == 3) {
    *lx = (int16_t)(479 - py);
    *ly = px;
  } else {
    *lx = py;
    *ly = (int16_t)(221 - px);
  }
}
