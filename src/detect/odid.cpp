#include "detect/odid.h"
#include <cstring>

namespace {
// Bounded little-endian readers: return false if the field lies outside buf.
bool rd_u8(const uint8_t* b, size_t len, size_t off, uint8_t& v) {
  if (off + 1 > len) return false;
  v = b[off];
  return true;
}
bool rd_u16le(const uint8_t* b, size_t len, size_t off, uint16_t& v) {
  if (off + 2 > len) return false;
  v = (uint16_t)(b[off] | (b[off + 1] << 8));
  return true;
}
bool rd_i32le(const uint8_t* b, size_t len, size_t off, int32_t& v) {
  if (off + 4 > len) return false;
  uint32_t u = (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8) | ((uint32_t)b[off + 2] << 16) |
               ((uint32_t)b[off + 3] << 24);
  v = (int32_t)u;
  return true;
}

float alt_m(uint16_t raw) { return raw * 0.5f - 1000.0f; }

bool latlon_valid(int32_t lat, int32_t lon) {
  if (lat == 0 && lon == 0) return false;  // ODID "unknown"
  return lat >= -900000000 && lat <= 900000000 && lon >= -1800000000 && lon <= 1800000000;
}

// Copies a fixed-width ODID text field, stopping at NUL, replacing non-printables.
void copy_text(char* dst, size_t cap, const uint8_t* src, size_t n) {
  size_t i = 0;
  for (; i < n && i + 1 < cap && src[i]; i++) {
    uint8_t c = src[i];
    dst[i] = (c >= 0x20 && c <= 0x7E) ? (char)c : '?';
  }
  dst[i] = '\0';
}

// Decodes exactly one 25-byte message (m has at least 25 bytes).
OdidErr decode_one(const uint8_t* m, OdidFields& f) {
  const size_t L = kOdidMsgSize;
  uint8_t type = m[0] >> 4;
  switch (type) {
    case 0x0: {  // Basic ID
      // Up to two Basic IDs may be sent; keep the serial number (id_type 1).
      if (f.has_basic && f.id_type == 1 && (m[1] >> 4) != 1) return OdidErr::OK;
      f.has_basic = true;
      f.id_type = m[1] >> 4;
      f.ua_type = m[1] & 0x0F;
      copy_text(f.uas_id, sizeof f.uas_id, m + 2, 20);
      return OdidErr::OK;
    }
    case 0x1: {  // Location / Vector
      uint8_t flags, dir, sh;
      uint16_t a_geo, h;
      int32_t lat, lon;
      rd_u8(m, L, 1, flags);
      rd_u8(m, L, 2, dir);
      rd_u8(m, L, 3, sh);
      rd_i32le(m, L, 5, lat);
      rd_i32le(m, L, 9, lon);
      rd_u16le(m, L, 15, a_geo);
      rd_u16le(m, L, 17, h);
      f.has_loc = true;
      f.status = flags >> 4;
      bool ew = (flags >> 1) & 1;
      bool mult = flags & 1;
      f.direction_deg = (float)dir + (ew ? 180.0f : 0.0f);
      f.speed_h_ms = mult ? sh * 0.75f + 63.75f : sh * 0.25f;
      f.speed_v_ms = (int8_t)m[4] * 0.5f;
      f.loc_valid = latlon_valid(lat, lon);
      f.lat = lat * 1e-7;
      f.lon = lon * 1e-7;
      f.alt_geo_m = alt_m(a_geo);
      f.height_m = alt_m(h);
      return OdidErr::OK;
    }
    case 0x2:  // Authentication — flagged only, not verified
      f.auth_present = true;
      return OdidErr::OK;
    case 0x3:  // Self-ID
      f.has_self = true;
      copy_text(f.self_id, sizeof f.self_id, m + 2, 23);
      return OdidErr::OK;
    case 0x4: {  // System
      int32_t lat, lon;
      uint16_t a;
      rd_i32le(m, L, 2, lat);
      rd_i32le(m, L, 6, lon);
      rd_u16le(m, L, 18, a);
      f.has_sys = true;
      f.op_valid = latlon_valid(lat, lon);
      f.op_lat = lat * 1e-7;
      f.op_lon = lon * 1e-7;
      f.op_alt_m = alt_m(a);
      return OdidErr::OK;
    }
    case 0x5:  // Operator ID
      f.has_opid = true;
      copy_text(f.operator_id, sizeof f.operator_id, m + 2, 20);
      return OdidErr::OK;
    default:
      return OdidErr::BAD_TYPE;
  }
}
}  // namespace

OdidErr odid_decode(const uint8_t* buf, size_t len, OdidFields& out) {
  if (!buf || len < 1) return OdidErr::TOO_SHORT;
  uint8_t type = buf[0] >> 4;
  if (type != 0xF) {
    if (len < kOdidMsgSize) return OdidErr::TOO_SHORT;
    return decode_one(buf, out);
  }
  // Message Pack
  uint8_t msz, count;
  if (!rd_u8(buf, len, 1, msz) || !rd_u8(buf, len, 2, count)) return OdidErr::TOO_SHORT;
  if (msz != kOdidMsgSize || count == 0 || count > 9) return OdidErr::BAD_PACK;
  if (len < 3 + (size_t)count * kOdidMsgSize) return OdidErr::TOO_SHORT;
  for (uint8_t i = 0; i < count; i++) {
    const uint8_t* m = buf + 3 + (size_t)i * kOdidMsgSize;
    if ((m[0] >> 4) == 0xF) continue;  // nested packs are not allowed; skip
    decode_one(m, out);                // unknown inner types are skipped
  }
  return OdidErr::OK;
}
