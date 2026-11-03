#pragma once
// Test-only OpenDroneID encoder (protocol version 2), inverse of odid_decode.
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstddef>

namespace enc {
inline void wr_u16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
inline void wr_i32(uint8_t* p, int32_t v) {
  uint32_t u = (uint32_t)v;
  for (int i = 0; i < 4; i++) p[i] = (u >> (8 * i)) & 0xFF;
}
inline uint16_t alt(float m) { return (uint16_t)lround((m + 1000.0f) / 0.5f); }
}  // namespace enc

inline void enc_basic(uint8_t* out, uint8_t id_type, uint8_t ua_type, const char* id) {
  memset(out, 0, 25);
  out[0] = (0x0 << 4) | 2;
  out[1] = (uint8_t)((id_type << 4) | (ua_type & 0x0F));
  strncpy((char*)out + 2, id, 20);
}

inline void enc_location(uint8_t* out, double lat, double lon, float alt_geo, float height,
                         float speed_h, float dir) {
  memset(out, 0, 25);
  out[0] = (0x1 << 4) | 2;
  uint8_t ew = dir >= 180.0f ? 1 : 0;
  uint8_t mult = speed_h > 63.75f ? 1 : 0;
  out[1] = (uint8_t)((2 << 4) | (ew << 1) | mult);  // status 2 = airborne
  out[2] = (uint8_t)lround(ew ? dir - 180.0f : dir);
  out[3] = (uint8_t)lround(mult ? (speed_h - 63.75f) / 0.75f : speed_h / 0.25f);
  out[4] = 0;
  enc::wr_i32(out + 5, (int32_t)llround(lat * 1e7));
  enc::wr_i32(out + 9, (int32_t)llround(lon * 1e7));
  enc::wr_u16(out + 13, enc::alt(alt_geo));
  enc::wr_u16(out + 15, enc::alt(alt_geo));
  enc::wr_u16(out + 17, enc::alt(height));
}

inline void enc_location_raw_lat(uint8_t* out, int32_t raw_lat, int32_t raw_lon) {
  enc_location(out, 0, 0, 0, 0, 0, 0);
  enc::wr_i32(out + 5, raw_lat);
  enc::wr_i32(out + 9, raw_lon);
}

inline void enc_system(uint8_t* out, double op_lat, double op_lon, float op_alt) {
  memset(out, 0, 25);
  out[0] = (0x4 << 4) | 2;
  out[1] = 1;  // operator location type: live GNSS
  enc::wr_i32(out + 2, (int32_t)llround(op_lat * 1e7));
  enc::wr_i32(out + 6, (int32_t)llround(op_lon * 1e7));
  enc::wr_u16(out + 18, enc::alt(op_alt));
}

inline void enc_operator(uint8_t* out, const char* id) {
  memset(out, 0, 25);
  out[0] = (0x5 << 4) | 2;
  strncpy((char*)out + 2, id, 20);
}

inline void enc_self_id(uint8_t* out, const char* text) {
  memset(out, 0, 25);
  out[0] = (0x3 << 4) | 2;
  strncpy((char*)out + 2, text, 23);
}

inline size_t enc_pack(uint8_t* out, const uint8_t* msgs, uint8_t count) {
  out[0] = (0xF << 4) | 2;
  out[1] = 25;
  out[2] = count;
  memcpy(out + 3, msgs, (size_t)count * 25);
  return 3 + (size_t)count * 25;
}

// Deterministic LCG for fuzz tests.
struct Lcg {
  uint32_t s;
  explicit Lcg(uint32_t seed) : s(seed) {}
  uint32_t next() { s = s * 1103515245u + 12345u; return s >> 8; }
};
