#include "ui/geo.h"
#include <cmath>

namespace {
constexpr double kEarthR = 6371008.8;  // mean Earth radius, m
constexpr double kDeg = 3.14159265358979323846 / 180.0;
float frac(float rssi) {  // -110 → 0, -40 → 1
  float t = (rssi + 110.0f) / 70.0f;
  return t < 0 ? 0 : (t > 1 ? 1 : t);
}
}  // namespace

double haversine_m(double lat1, double lon1, double lat2, double lon2) {
  double dlat = (lat2 - lat1) * kDeg, dlon = (lon2 - lon1) * kDeg;
  double a = sin(dlat / 2) * sin(dlat / 2) +
             cos(lat1 * kDeg) * cos(lat2 * kDeg) * sin(dlon / 2) * sin(dlon / 2);
  return 2 * kEarthR * atan2(sqrt(a), sqrt(1 - a));
}

bool drone_pilot_distance_m(const OdidFields& f, double* out_m) {
  if (!f.loc_valid || !f.op_valid) return false;
  *out_m = haversine_m(f.lat, f.lon, f.op_lat, f.op_lon);
  return true;
}

uint16_t rssi_to_pulse_hz_x10(float rssi) { return (uint16_t)lroundf(10.0f + 90.0f * frac(rssi)); }
uint16_t rssi_to_tone_hz(float rssi) { return (uint16_t)lroundf(400.0f + 1600.0f * frac(rssi)); }
