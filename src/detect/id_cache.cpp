#include "detect/id_cache.h"
#include <cstdio>
#include <cstring>

void format_mac(const uint8_t m[6], char out[18]) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

void IdCache::remember(const uint8_t mac[6], const char* serial, uint32_t now_ms, bool is_serial) {
  if (!serial || !serial[0]) return;
  Entry* slot = nullptr;
  for (Entry& e : e_)
    if (e.used && memcmp(e.mac, mac, 6) == 0) { slot = &e; break; }
  if (slot && slot->is_serial && !is_serial) { slot->t_ms = now_ms; return; }
  if (!slot)
    for (Entry& e : e_)
      if (!e.used) { slot = &e; break; }
  if (!slot) {
    slot = &e_[0];
    for (Entry& e : e_)
      if (e.t_ms < slot->t_ms) slot = &e;
  }
  memcpy(slot->mac, mac, 6);
  strncpy(slot->serial, serial, sizeof slot->serial - 1);
  slot->serial[sizeof slot->serial - 1] = '\0';
  slot->t_ms = now_ms;
  slot->used = true;
  slot->is_serial = is_serial;
}

bool IdCache::resolve(const uint8_t mac[6], char out[24]) const {
  for (const Entry& e : e_) {
    if (e.used && memcmp(e.mac, mac, 6) == 0) {
      snprintf(out, 24, "%s", e.serial);
      return true;
    }
  }
  format_mac(mac, out);
  return false;
}
