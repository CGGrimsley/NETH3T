#pragma once
// Remembers which Remote ID serial a transmitter MAC announced, so messages
// that carry no serial (Location, System, …) merge into the same target. Pure.
#include <cstddef>
#include <cstdint>

void format_mac(const uint8_t mac[6], char out[18]);  // "AA:BB:CC:DD:EE:FF"

class IdCache {
 public:
  static constexpr size_t kSize = 32;
  // `is_serial` = the ID is a serial number (ODID id_type 1); a serial is never
  // replaced by a non-serial ID for the same MAC.
  void remember(const uint8_t mac[6], const char* serial, uint32_t now_ms, bool is_serial = true);
  // Writes the serial if known, else the formatted MAC. Returns true if a serial was found.
  bool resolve(const uint8_t mac[6], char out[24]) const;

 private:
  struct Entry { uint8_t mac[6]; char serial[21]; uint32_t t_ms; bool used; bool is_serial; };
  Entry e_[kSize] = {};
};
