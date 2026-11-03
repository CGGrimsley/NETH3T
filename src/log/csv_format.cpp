#include "log/csv_format.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

const char* const kCsvHeader =
    "t_ms,iso_time,type,conf,id,freq_khz,rssi,snr,sf,bw_khz,lat,lon,alt_m,op_lat,op_lon,vendor,extra";

const char* source_type_name(SourceType t) {
  switch (t) {
    case SourceType::REMOTEID_WIFI: return "REMOTEID_WIFI";
    case SourceType::REMOTEID_BLE: return "REMOTEID_BLE";
    case SourceType::DJI_LEGACY: return "DJI_LEGACY";
    case SourceType::DRONE_WIFI: return "DRONE_WIFI";
    case SourceType::ELRS: return "ELRS";
    case SourceType::FHSS: return "FHSS";
    case SourceType::MESHTASTIC: return "MESHTASTIC";
    case SourceType::LORAWAN: return "LORAWAN";
    case SourceType::LORA_UNKNOWN: return "LORA_UNKNOWN";
  }
  return "?";
}

const char* conf_name(Confidence c) {
  switch (c) {
    case Confidence::DECODED: return "DECODED";
    case Confidence::FINGERPRINT: return "FINGERPRINT";
    case Confidence::HEURISTIC: return "HEURISTIC";
  }
  return "?";
}

namespace {
struct Out {
  char* p;
  size_t cap, n;
  bool overflow;
  void raw(const char* s, size_t len) {
    if (n + len >= cap) { overflow = true; return; }
    memcpy(p + n, s, len);
    n += len;
    p[n] = '\0';
  }
  void fmt(const char* f, ...) {
    char tmp[64];
    va_list ap;
    va_start(ap, f);
    int k = vsnprintf(tmp, sizeof tmp, f, ap);
    va_end(ap);
    if (k > 0) raw(tmp, (size_t)k);
  }
  void comma() { raw(",", 1); }
  void field(const char* s) {  // RFC 4180 quoting when needed
    if (!strpbrk(s, ",\"\r\n")) { raw(s, strlen(s)); return; }
    raw("\"", 1);
    for (const char* c = s; *c; c++) {
      if (*c == '"') raw("\"\"", 2);
      else raw(c, 1);
    }
    raw("\"", 1);
  }
};
}  // namespace

size_t csv_row(const Observation& o, const char* iso_time, char* out, size_t cap) {
  if (!out || cap == 0) return 0;
  Out w{out, cap, 0, false};
  out[0] = '\0';
  w.fmt("%lu", (unsigned long)o.t_ms); w.comma();
  w.field(iso_time ? iso_time : ""); w.comma();
  w.field(source_type_name(o.type)); w.comma();
  w.field(conf_name(o.conf)); w.comma();
  w.field(o.id); w.comma();
  w.fmt("%lu", (unsigned long)o.freq_khz); w.comma();
  w.fmt("%d", o.rssi_dbm); w.comma();
  w.fmt("%d", o.snr_db); w.comma();
  if (o.lora.sf) { w.fmt("%u", o.lora.sf); w.comma(); w.fmt("%.1f", o.lora.bw_hz / 1000.0); w.comma(); }
  else { w.comma(); w.comma(); }
  if (o.odid.has_loc && o.odid.loc_valid) {
    w.fmt("%.7f", o.odid.lat); w.comma();
    w.fmt("%.7f", o.odid.lon); w.comma();
    w.fmt("%.1f", (double)o.odid.alt_geo_m); w.comma();
  } else { w.comma(); w.comma(); w.comma(); }
  if (o.odid.has_sys && o.odid.op_valid) {
    w.fmt("%.7f", o.odid.op_lat); w.comma();
    w.fmt("%.7f", o.odid.op_lon); w.comma();
  } else { w.comma(); w.comma(); }
  w.field(o.vendor); w.comma();
  w.field(o.label);
  if (w.overflow) { out[0] = '\0'; return 0; }
  return w.n;
}
