#pragma once
// Drone-maker identification from MAC OUI and SSID patterns. Pure. HEURISTIC only.
#include <cstdint>

const char* drone_vendor_by_oui(const uint8_t mac[6]);  // vendor name or nullptr
const char* drone_vendor_by_ssid(const char* ssid);     // vendor name or nullptr
