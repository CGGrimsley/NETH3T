#include "detect/channel_plans.h"
#include <cstring>
#include <initializer_list>

namespace {
constexpr uint8_t kMaxPoints = 80;

uint32_t djb2(const char* s) {
  uint32_t h = 5381;
  for (; *s; s++) h = h * 33u + (uint8_t)*s;
  return h;
}

ScanPlan blank(const char* name, PlanMode mode, PlanKind kind) {
  ScanPlan p;
  memset(&p, 0, sizeof p);
  p.name = name;
  p.mode = mode;
  p.kind = kind;
  return p;
}

// Evenly spaced sweep from f0 to f1 with `step` kHz, capped at 80 points
// (stride widened to fit when capped).
ScanPlan sweep(const char* name, uint32_t f0, uint32_t f1, uint32_t step, bool hf) {
  ScanPlan p = blank(name, PlanMode::RSSI_SWEEP, PlanKind::SWEEP);
  p.hf = hf;
  if (f1 <= f0 || step == 0) {  // degenerate range: one point, never divide by zero
    p.freqs_khz[0] = f0;
    p.n_freqs = 1;
    return p;
  }
  uint32_t span = f1 - f0;
  uint32_t n = span / step + 1;
  if (n > kMaxPoints) n = kMaxPoints;
  for (uint32_t i = 0; i < n; i++) p.freqs_khz[i] = f0 + (uint32_t)((uint64_t)span * i / (n - 1));
  p.n_freqs = (uint8_t)n;
  p.hf = hf;
  return p;
}

ScanPlan linear(const char* name, PlanMode mode, PlanKind kind, uint32_t f0, uint32_t step, uint8_t n) {
  ScanPlan p = blank(name, mode, kind);
  for (uint8_t i = 0; i < n; i++) p.freqs_khz[i] = f0 + step * i;
  p.n_freqs = n;
  return p;
}

void set_sfs(ScanPlan& p, std::initializer_list<uint8_t> sfs) {
  p.n_sfs = 0;
  for (uint8_t s : sfs) p.sfs[p.n_sfs++] = s;
}

struct MeshPreset { const char* plan_name; const char* preset; uint8_t sf; uint32_t bw; };
const MeshPreset kMesh[] = {
    {"MESH LongFast", "LongFast", 11, 250000},
    {"MESH MediumFast", "MediumFast", 9, 250000},
    {"MESH ShortFast", "ShortFast", 7, 250000},
    {"MESH LongSlow", "LongSlow", 12, 125000},
};
}  // namespace

ScanPlan make_sweep_plan(const char* name, uint32_t f0, uint32_t f1, uint32_t min_step, bool hf) {
  return sweep(name, f0, f1, min_step, hf);
}

uint32_t meshtastic_freq_khz(Region r, const char* preset_name, uint32_t bw_hz) {
  uint64_t start = r == Region::FCC ? 902000000ULL : 869400000ULL;
  uint64_t end = r == Region::FCC ? 928000000ULL : 869650000ULL;
  uint32_t n = (uint32_t)((end - start) / bw_hz);
  if (n == 0) n = 1;
  uint32_t slot = djb2(preset_name) % n;
  uint64_t f = start + bw_hz / 2 + (uint64_t)slot * bw_hz;
  return (uint32_t)(f / 1000);
}

uint16_t elrs_rate_hz(bool band24, uint8_t sf) {
  if (band24) {
    switch (sf) { case 5: return 500; case 6: return 250; case 7: return 150; case 8: return 50; }
  } else {
    switch (sf) { case 6: return 200; case 7: return 100; case 8: return 50; case 9: return 25; }
  }
  return 0;
}

size_t build_plans(Region r, ScanPlan* out, size_t max) {
  ScanPlan all[kMaxPlans];
  size_t n = 0;
  bool fcc = r == Region::FCC;

  all[n++] = fcc ? sweep("SWEEP 915", 902000, 928000, 200, false)
                 : sweep("SWEEP 868", 863000, 870000, 200, false);

  ScanPlan e9 = fcc ? linear("ELRS900", PlanMode::CAD, PlanKind::ELRS, 903500, 600, 40)
                    : linear("ELRS900", PlanMode::CAD, PlanKind::ELRS, 863275, 525, 13);
  e9.bw_hz = 500000;
  set_sfs(e9, {6, 7, 8, 9});
  all[n++] = e9;

  for (const MeshPreset& m : kMesh) {
    ScanPlan p = blank(m.plan_name, PlanMode::RX, PlanKind::MESHTASTIC);
    p.freqs_khz[0] = meshtastic_freq_khz(r, m.preset, m.bw);
    p.n_freqs = 1;
    set_sfs(p, {m.sf});
    p.bw_hz = m.bw;
    p.sync_word = 0x2B;
    p.rx_timeout_ms = 1500;
    all[n++] = p;
  }

  all[n++] = sweep("SWEEP 2400", 2400000, 2479000, 1000, true);

  ScanPlan e24 = linear("ELRS24", PlanMode::CAD, PlanKind::ELRS, 2400400, 1000, 80);
  e24.bw_hz = 812500;
  e24.hf = true;
  set_sfs(e24, {5, 6, 7, 8});
  all[n++] = e24;

  ScanPlan lw = blank("LORAWAN", PlanMode::RX, PlanKind::LORAWAN);
  if (fcc) {
    for (uint8_t i = 0; i < 8; i++) lw.freqs_khz[i] = 903900 + 200 * i;
    lw.n_freqs = 8;
  } else {
    lw.freqs_khz[0] = 868100; lw.freqs_khz[1] = 868300; lw.freqs_khz[2] = 868500;
    lw.n_freqs = 3;
  }
  set_sfs(lw, {7, 8, 9, 10});
  lw.bw_hz = 125000;
  lw.sync_word = 0x34;
  lw.rx_timeout_ms = 1000;
  all[n++] = lw;

  all[n++] = sweep("SWEEP 433", 433050, 434790, 200, false);

  size_t k = n < max ? n : max;
  for (size_t i = 0; i < k; i++) out[i] = all[i];
  return k;
}
