#pragma once
// Target table: merges observations, smooths RSSI, computes trend, ages targets.
// Pure (no RTOS) — db_task.cpp wraps it with a mutex and queue.
#include "core/observation.h"

struct Target {
  SourceType type;
  Confidence conf;
  char id[24], label[24], vendor[16];
  uint32_t freq_khz;
  int16_t last_rssi, peak_rssi;
  int8_t snr_db;
  float ema_rssi;
  uint32_t first_ms, last_ms, hits;
  bool stale, alerted;
  struct Sample { uint32_t t_ms; float ema; } hist[64];
  uint8_t hist_head, hist_len;
  OdidFields odid;
  LoraFields lora;
};

class TargetDb {
 public:
  static constexpr size_t kCapacity = 128;
  static constexpr uint32_t kStaleMs = 20000, kExpireMs = 60000, kTrendWindowMs = 3000;
  static constexpr float kEmaAlpha = 0.3f, kTrendDeadbandDb = 2.0f;

  void ingest(const Observation& o);
  void tick(uint32_t now_ms);
  size_t snapshot(Target* out, size_t max) const;  // sorted: conf desc, then ema_rssi desc
  Target* find(SourceType t, const char* id);
  bool lock(SourceType t, const char* id);
  void unlock();
  const Target* locked() const;
  void reset_peak(SourceType t, const char* id);
  size_t size() const { return count_; }

 private:
  Target slots_[kCapacity];
  bool used_[kCapacity] = {};
  size_t count_ = 0;
  int locked_ = -1;
  int find_index(SourceType t, const char* id) const;
  int alloc_slot();
};

Trend trend_of(const Target& t, uint32_t now_ms);
