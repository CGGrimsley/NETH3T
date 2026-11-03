#pragma once
// Classifies ELRS and generic FHSS RC links from CAD hits and hot RSSI bins. Pure.
#include <cstddef>
#include <cstdint>
#include "core/observation.h"

struct CadHit { uint32_t freq_khz; uint8_t sf; uint32_t bw_hz; uint32_t t_ms; int16_t rssi; };
struct HotBin { uint32_t freq_khz; uint32_t t_ms; int16_t rssi; };

class RcFingerprint {
 public:
  static constexpr uint32_t kWindowMs = 2000;
  static constexpr size_t kRing = 128;
  void add_cad(const CadHit& h);
  void add_hot(const HotBin& b);
  // Emits ELRS / FHSS observations with synthetic ids
  // ("ELRS900-SF7", "ELRS24-SF6", "FHSS915", "FHSS868").
  size_t evaluate(uint32_t now_ms, Observation* out, size_t max);
  size_t recent_cad(uint32_t now_ms, CadHit* out, size_t max) const;

 private:
  CadHit cad_[kRing];
  size_t cad_n_ = 0, cad_head_ = 0;
  HotBin hot_[kRing];
  size_t hot_n_ = 0, hot_head_ = 0;
  // Newest hit already reported per ELRS (bw, sf) group and per FHSS band, so a
  // window is only re-reported when new hits arrived.
  struct Emitted { uint32_t bw_hz; uint8_t sf; uint32_t t_ms; };
  Emitted emitted_[16] = {};
  size_t n_emitted_ = 0;
  uint32_t fhss_emitted_t_[2] = {};
  bool fhss_emitted_[2] = {};
  bool should_emit(uint32_t bw, uint8_t sf, uint32_t newest_t);
};
