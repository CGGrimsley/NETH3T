#include "core/target_db.h"
#include <cstring>

namespace {
// Remote ID over Wi-Fi and BLE describe the same aircraft: merge them by serial.
int merge_class(SourceType t) {
  return t == SourceType::REMOTEID_BLE ? (int)SourceType::REMOTEID_WIFI : (int)t;
}
// History keeps at most one sample per kHistSpacingMs so 64 entries cover ~32 s.
constexpr uint32_t kHistSpacingMs = 500;

void copy_str(char* dst, const char* src, size_t cap) {
  if (!src[0]) return;
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
}

void merge_odid(OdidFields& d, const OdidFields& s) {
  if (s.has_basic) {
    d.has_basic = true; d.id_type = s.id_type; d.ua_type = s.ua_type;
    memcpy(d.uas_id, s.uas_id, sizeof d.uas_id);
  }
  if (s.has_loc) {
    d.has_loc = true; d.status = s.status; d.loc_valid = s.loc_valid;
    d.lat = s.lat; d.lon = s.lon; d.alt_geo_m = s.alt_geo_m; d.height_m = s.height_m;
    d.speed_h_ms = s.speed_h_ms; d.speed_v_ms = s.speed_v_ms; d.direction_deg = s.direction_deg;
  }
  if (s.has_sys) {
    d.has_sys = true; d.op_valid = s.op_valid;
    d.op_lat = s.op_lat; d.op_lon = s.op_lon; d.op_alt_m = s.op_alt_m;
  }
  if (s.has_opid) { d.has_opid = true; memcpy(d.operator_id, s.operator_id, sizeof d.operator_id); }
  if (s.has_self) { d.has_self = true; memcpy(d.self_id, s.self_id, sizeof d.self_id); }
  if (s.auth_present) d.auth_present = true;
}

void push_hist(Target& t, uint32_t now, float ema) {
  if (t.hist_len > 0) {
    uint8_t last = (uint8_t)((t.hist_head + 63) % 64);
    if (now - t.hist[last].t_ms < kHistSpacingMs) { t.hist[last].ema = ema; return; }
  }
  t.hist[t.hist_head] = {now, ema};
  t.hist_head = (uint8_t)((t.hist_head + 1) % 64);
  if (t.hist_len < 64) t.hist_len++;
}

bool before(const Target& a, const Target& b) {  // snapshot order
  if (a.conf != b.conf) return a.conf > b.conf;
  return a.ema_rssi > b.ema_rssi;
}
}  // namespace

int TargetDb::find_index(SourceType t, const char* id) const {
  for (size_t i = 0; i < kCapacity; i++)
    if (used_[i] && merge_class(slots_[i].type) == merge_class(t) && strcmp(slots_[i].id, id) == 0)
      return (int)i;
  return -1;
}

int TargetDb::alloc_slot() {
  for (size_t i = 0; i < kCapacity; i++)
    if (!used_[i]) return (int)i;
  // Full: evict lowest confidence, then oldest last_ms; never the locked one.
  int victim = -1;
  for (size_t i = 0; i < kCapacity; i++) {
    if ((int)i == locked_) continue;
    if (victim < 0) { victim = (int)i; continue; }
    const Target& a = slots_[i];
    const Target& v = slots_[victim];
    if (a.conf < v.conf || (a.conf == v.conf && a.last_ms < v.last_ms)) victim = (int)i;
  }
  if (victim >= 0) { used_[victim] = false; count_--; }
  return victim;
}

void TargetDb::ingest(const Observation& o) {
  int i = find_index(o.type, o.id);
  if (i < 0) {
    i = alloc_slot();
    if (i < 0) return;
    Target& t = slots_[i];
    memset(&t, 0, sizeof t);
    t.type = o.type; t.conf = o.conf;
    copy_str(t.id, o.id, sizeof t.id);
    t.first_ms = o.t_ms;
    t.ema_rssi = o.rssi_dbm;
    t.peak_rssi = o.rssi_dbm;
    used_[i] = true;
    count_++;
  } else {
    Target& t = slots_[i];
    t.ema_rssi = kEmaAlpha * o.rssi_dbm + (1.0f - kEmaAlpha) * t.ema_rssi;
    if (o.rssi_dbm > t.peak_rssi) t.peak_rssi = o.rssi_dbm;
    if (o.conf > t.conf) t.conf = o.conf;
    t.type = o.type;
  }
  Target& t = slots_[i];
  t.last_rssi = o.rssi_dbm;
  t.snr_db = o.snr_db;
  t.last_ms = o.t_ms;
  t.hits++;
  t.stale = false;
  if (o.freq_khz) t.freq_khz = o.freq_khz;
  copy_str(t.label, o.label, sizeof t.label);
  copy_str(t.vendor, o.vendor, sizeof t.vendor);
  if (o.lora.sf) t.lora = o.lora;
  merge_odid(t.odid, o.odid);
  push_hist(t, o.t_ms, t.ema_rssi);
}

void TargetDb::tick(uint32_t now_ms) {
  for (size_t i = 0; i < kCapacity; i++) {
    if (!used_[i]) continue;
    uint32_t age = now_ms - slots_[i].last_ms;
    slots_[i].stale = age > kStaleMs;
    if (age > kExpireMs && (int)i != locked_) { used_[i] = false; count_--; }
  }
}

size_t TargetDb::snapshot(Target* out, size_t max) const {
  size_t n = 0;
  for (size_t i = 0; i < kCapacity && n < max; i++) {
    if (!used_[i]) continue;
    // insertion sort into out[0..n]
    size_t j = n++;
    while (j > 0 && before(slots_[i], out[j - 1])) { out[j] = out[j - 1]; j--; }
    out[j] = slots_[i];
  }
  return n;
}

Target* TargetDb::find(SourceType t, const char* id) {
  int i = find_index(t, id);
  return i < 0 ? nullptr : &slots_[i];
}

bool TargetDb::lock(SourceType t, const char* id) {
  int i = find_index(t, id);
  if (i < 0) return false;
  locked_ = i;
  return true;
}

void TargetDb::unlock() { locked_ = -1; }

const Target* TargetDb::locked() const {
  return (locked_ >= 0 && used_[locked_]) ? &slots_[locked_] : nullptr;
}

void TargetDb::reset_peak(SourceType t, const char* id) {
  Target* p = find(t, id);
  if (p) p->peak_rssi = p->last_rssi;
}

Trend trend_of(const Target& t, uint32_t now_ms) {
  if (t.hist_len == 0) return Trend::STEADY;
  // Walk newest → oldest; reference = newest sample at least the window old, else oldest.
  const Target::Sample* ref = nullptr;
  for (uint8_t k = 0; k < t.hist_len; k++) {
    const Target::Sample& s = t.hist[(t.hist_head + 64 - 1 - k) % 64];
    ref = &s;
    if (now_ms - s.t_ms >= TargetDb::kTrendWindowMs) break;
  }
  float d = t.ema_rssi - ref->ema;
  if (d >= TargetDb::kTrendDeadbandDb) return Trend::WARMER;
  if (d <= -TargetDb::kTrendDeadbandDb) return Trend::COLDER;
  return Trend::STEADY;
}
