#include "detect/rc_fingerprint.h"
#include <cstdio>
#include <cstring>
#include "detect/channel_plans.h"

namespace {
bool in_window(uint32_t now, uint32_t t) { return now - t <= RcFingerprint::kWindowMs; }
bool is_24(uint32_t bw_hz) { return bw_hz > 700000; }
bool in915(uint32_t f) { return f >= 902000 && f <= 928000; }
bool in868(uint32_t f) { return f >= 863000 && f <= 870000; }

bool contains(const uint32_t* a, size_t n, uint32_t v) {
  for (size_t i = 0; i < n; i++)
    if (a[i] == v) return true;
  return false;
}

Observation make_obs(SourceType type, Confidence conf, uint32_t now) {
  Observation o;
  memset(&o, 0, sizeof o);
  o.type = type;
  o.conf = conf;
  o.t_ms = now;
  return o;
}
}  // namespace

void RcFingerprint::add_cad(const CadHit& h) {
  cad_[cad_head_] = h;
  cad_head_ = (cad_head_ + 1) % kRing;
  if (cad_n_ < kRing) cad_n_++;
}

void RcFingerprint::add_hot(const HotBin& b) {
  hot_[hot_head_] = b;
  hot_head_ = (hot_head_ + 1) % kRing;
  if (hot_n_ < kRing) hot_n_++;
}

size_t RcFingerprint::recent_cad(uint32_t now_ms, CadHit* out, size_t max) const {
  size_t n = 0;
  for (size_t i = 0; i < cad_n_ && n < max; i++)
    if (in_window(now_ms, cad_[i].t_ms)) out[n++] = cad_[i];
  return n;
}

bool RcFingerprint::should_emit(uint32_t bw, uint8_t sf, uint32_t newest_t) {
  for (size_t i = 0; i < n_emitted_; i++) {
    if (emitted_[i].bw_hz == bw && emitted_[i].sf == sf) {
      if (newest_t == emitted_[i].t_ms) return false;
      emitted_[i].t_ms = newest_t;
      return true;
    }
  }
  if (n_emitted_ < sizeof emitted_ / sizeof emitted_[0]) emitted_[n_emitted_++] = {bw, sf, newest_t};
  return true;
}

size_t RcFingerprint::evaluate(uint32_t now_ms, Observation* out, size_t max) {
  size_t n_out = 0;
  CadHit win[kRing];
  size_t nw = recent_cad(now_ms, win, kRing);
  bool done[kRing] = {};
  bool elrs915 = false, elrs868 = false;

  // ELRS: group CAD hits by (bw, sf)
  for (size_t i = 0; i < nw && n_out < max; i++) {
    if (done[i]) continue;
    uint32_t freqs[kRing];
    size_t nf = 0, hits = 0;
    int16_t newest_rssi = 0;
    uint32_t last_f = 0, last_t = 0;
    bool any = false;
    for (size_t j = i; j < nw; j++) {
      if (done[j] || win[j].bw_hz != win[i].bw_hz || win[j].sf != win[i].sf) continue;
      done[j] = true;
      hits++;
      if (!contains(freqs, nf, win[j].freq_khz)) freqs[nf++] = win[j].freq_khz;
      if (!any || win[j].t_ms >= last_t) {
        any = true;
        last_t = win[j].t_ms;
        last_f = win[j].freq_khz;
        newest_rssi = win[j].rssi;  // hunting wants the current level, not a window peak
      }
    }
    if (hits < 4 || nf < 3) continue;
    bool b24 = is_24(win[i].bw_hz);
    if (!b24 && in915(last_f)) elrs915 = true;
    if (!b24 && in868(last_f)) elrs868 = true;
    if (!should_emit(win[i].bw_hz, win[i].sf, last_t)) continue;
    Observation o = make_obs(SourceType::ELRS, Confidence::FINGERPRINT, now_ms);
    uint16_t rate = elrs_rate_hz(b24, win[i].sf);
    snprintf(o.id, sizeof o.id, "%s-SF%u", b24 ? "ELRS24" : "ELRS900", (unsigned)win[i].sf);
    const char* band = b24 ? "2.4G" : (last_f < 900000 ? "868" : "915");
    if (rate) snprintf(o.label, sizeof o.label, "ELRS %s ~%uHz", band, (unsigned)rate);
    else snprintf(o.label, sizeof o.label, "ELRS %s SF%u", band, (unsigned)win[i].sf);
    o.freq_khz = last_f;
    o.rssi_dbm = newest_rssi;
    o.lora = {win[i].sf, win[i].bw_hz, rate};
    out[n_out++] = o;
  }

  // FHSS: hot bins hopping across a sub-GHz ISM band
  struct Band { const char* id; bool (*in)(uint32_t); bool suppressed; };
  Band bands[2] = {{"FHSS915", in915, elrs915}, {"FHSS868", in868, elrs868}};
  for (size_t bi = 0; bi < 2; bi++) {
    Band& b = bands[bi];
    if (b.suppressed || n_out >= max) continue;
    uint32_t freqs[kRing];
    uint16_t counts[kRing];
    size_t nf = 0, total = 0;
    int16_t best = 0;
    uint32_t last_f = 0, last_t = 0;
    bool any = false;
    for (size_t i = 0; i < hot_n_; i++) {
      const HotBin& h = hot_[i];
      if (!in_window(now_ms, h.t_ms) || !b.in(h.freq_khz)) continue;
      total++;
      if (!any || h.t_ms >= last_t) { any = true; last_t = h.t_ms; best = h.rssi; last_f = h.freq_khz; }
      size_t k = 0;
      while (k < nf && freqs[k] != h.freq_khz) k++;
      if (k == nf) { freqs[nf] = h.freq_khz; counts[nf] = 0; nf++; }
      counts[k]++;
    }
    if (nf < 6) continue;
    uint16_t top = 0;
    for (size_t k = 0; k < nf; k++)
      if (counts[k] > top) top = counts[k];
    if (top * 10 > total * 4) continue;  // one frequency > 40 % → static carrier
    if (fhss_emitted_[bi] && fhss_emitted_t_[bi] == last_t) continue;  // nothing new
    fhss_emitted_[bi] = true;
    fhss_emitted_t_[bi] = last_t;
    Observation o = make_obs(SourceType::FHSS, Confidence::HEURISTIC, now_ms);
    snprintf(o.id, sizeof o.id, "%s", b.id);
    snprintf(o.label, sizeof o.label, "FHSS RC %s", b.id + 4);
    o.freq_khz = last_f;
    o.rssi_dbm = best;
    out[n_out++] = o;
  }
  return n_out;
}
