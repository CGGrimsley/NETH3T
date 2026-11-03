#pragma once
// Region-specific LR1121 scan plans (sweeps, ELRS CAD, Meshtastic/LoRaWAN RX). Pure.
#include <cstddef>
#include <cstdint>

enum class Region : uint8_t { FCC, EU };
enum class PlanMode : uint8_t { RSSI_SWEEP, CAD, RX };
enum class PlanKind : uint8_t { SWEEP, ELRS, MESHTASTIC, LORAWAN };

struct ScanPlan {
  const char* name;
  PlanMode mode;
  PlanKind kind;
  uint32_t freqs_khz[80];
  uint8_t n_freqs;
  uint8_t sfs[4];
  uint8_t n_sfs;
  uint32_t bw_hz;
  uint8_t sync_word;
  uint16_t rx_timeout_ms;
  bool hf;  // 2.4 GHz
};

constexpr size_t kMaxPlans = 12;
size_t build_plans(Region r, ScanPlan* out, size_t max);  // returns count
uint32_t meshtastic_freq_khz(Region r, const char* preset_name, uint32_t bw_hz);
uint16_t elrs_rate_hz(bool band24, uint8_t sf);  // 0 if unknown
