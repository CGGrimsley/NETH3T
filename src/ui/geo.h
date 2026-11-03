#pragma once
// Pure helpers for the Hunt screen.
#include <cstdint>
#include "core/observation.h"

double haversine_m(double lat1, double lon1, double lat2, double lon2);
bool drone_pilot_distance_m(const OdidFields& f, double* out_m);  // false unless loc_valid && op_valid
uint16_t rssi_to_pulse_hz_x10(float rssi);  // -110 dBm → 10 (1 Hz) … -40 → 100 (10 Hz), clamped, linear
uint16_t rssi_to_tone_hz(float rssi);       // -110 → 400 … -40 → 2000, clamped, linear
