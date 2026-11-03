#include "detect/drone_vendors.h"
#include <cctype>
#include <cstddef>
#include <cstring>

namespace {
struct OuiEntry { uint32_t oui; const char* vendor; };

// Source: IEEE MA-L registry (standards-oui.ieee.org/oui/oui.csv), checked 2026-11-03.
// DJI = "SZ DJI TECHNOLOGY CO." and "DJI BAIWANG TECHNOLOGY CO LTD" (aircraft makers).
// Excluded on purpose: DJI Ronin / DJI Osmo (gimbals, cameras), "Beijing Autelan"
// (unrelated to Autel Robotics). Sorted ascending for binary search.
const OuiEntry kOuis[] = {
    {0x00121C, "Parrot"}, {0x00267E, "Parrot"}, {0x04A85A, "DJI"},    {0x0C9AE6, "DJI"},
    {0x3491F0, "DJI"},    {0x34D262, "DJI"},    {0x381D14, "Skydio"}, {0x481CB9, "DJI"},
    {0x4C43F6, "DJI"},    {0x58B858, "DJI"},    {0x60601F, "DJI"},    {0x882985, "DJI"},
    {0x8C5823, "DJI"},    {0x9003B7, "Parrot"}, {0x903AE6, "Parrot"}, {0x9C5A8A, "DJI"},
    {0xA0143D, "Parrot"}, {0xC8A162, "DJI"},    {0xE47A2C, "DJI"},    {0xEC72F7, "DJI"},
};

enum class Rule : uint8_t { PREFIX, CONTAINS, EVO };
struct SsidRule { Rule rule; const char* pat; const char* vendor; };
const SsidRule kSsids[] = {
    {Rule::PREFIX, "TELLO-", "Ryze/DJI Tello"}, {Rule::PREFIX, "DJI-", "DJI"},
    {Rule::CONTAINS, "MAVIC", "DJI"},           {Rule::PREFIX, "SPARK-", "DJI"},
    {Rule::CONTAINS, "PHANTOM", "DJI"},         {Rule::CONTAINS, "ANAFI", "Parrot"},
    {Rule::PREFIX, "BEBOP", "Parrot"},          {Rule::PREFIX, "DISCO-", "Parrot"},
    {Rule::PREFIX, "AUTEL", "Autel"},           {Rule::EVO, "EVO", "Autel"},
    {Rule::PREFIX, "SKYDIO", "Skydio"},         {Rule::PREFIX, "YUNEEC", "Yuneec"},
    {Rule::PREFIX, "HUBSAN", "Hubsan"},         {Rule::PREFIX, "HOLYSTONE", "Holy Stone"},
    {Rule::PREFIX, "HS-", "Holy Stone"},
};
}  // namespace

const char* drone_vendor_by_oui(const uint8_t mac[6]) {
  if (!mac) return nullptr;
  uint32_t key = ((uint32_t)mac[0] << 16) | ((uint32_t)mac[1] << 8) | mac[2];
  size_t lo = 0, hi = sizeof kOuis / sizeof kOuis[0];
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    if (kOuis[mid].oui == key) return kOuis[mid].vendor;
    if (kOuis[mid].oui < key) lo = mid + 1;
    else hi = mid;
  }
  return nullptr;
}

const char* drone_vendor_by_ssid(const char* ssid) {
  if (!ssid || !ssid[0]) return nullptr;
  char up[33];
  size_t n = 0;
  for (; ssid[n] && n < 32; n++) up[n] = (char)toupper((unsigned char)ssid[n]);
  up[n] = '\0';
  for (const SsidRule& r : kSsids) {
    size_t pl = strlen(r.pat);
    switch (r.rule) {
      case Rule::PREFIX:
        if (strncmp(up, r.pat, pl) == 0) return r.vendor;
        break;
      case Rule::CONTAINS:
        if (strstr(up, r.pat)) return r.vendor;
        break;
      case Rule::EVO:
        if (strncmp(up, "EVO", 3) == 0 &&
            (up[3] == '_' || up[3] == '-' || (up[3] == 'I' && up[4] == 'I')))
          return r.vendor;
        break;
    }
  }
  return nullptr;
}
