#pragma once
// Remembers which Remote ID serial a transmitter MAC announced, so messages
// that carry no serial (Location, System, …) merge into the same target. Pure.
#include <cstddef>
#include <cstdint>

void format_mac(const uint8_t mac[6], char out[18]);  // "AA:BB:CC:DD:EE:FF"

class IdCache {
 public:
  static constexpr size_t kSize = 32;
  void remember(const uint8_t mac[6], const char* serial, uint32_t now_ms);
  // Writes the serial if known, else the formatted MAC. Returns true if a serial was found.
  bool resolve(const uint8_t mac[6], char out[24]) const;

 private:
  struct Entry { uint8_t mac[6]; char serial[21]; uint32_t t_ms; bool used; };
  Entry e_[kSize] = {};
};
