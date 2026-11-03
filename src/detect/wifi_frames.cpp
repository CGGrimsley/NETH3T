#include "detect/wifi_frames.h"
#include <cstring>

const uint8_t kNanOdidServiceId[6] = {0x88, 0x69, 0x19, 0x9D, 0x92, 0x09};

namespace {
constexpr size_t kHdrLen = 24;
constexpr size_t kBeaconFixed = 12;
const uint8_t kOdidOui[4] = {0xFA, 0x0B, 0xBC, 0x0D};
const uint8_t kDjiOui[3] = {0x26, 0x37, 0x12};
const uint8_t kNanDst[6] = {0x51, 0x6F, 0x9A, 0x01, 0x00, 0x00};
const uint8_t kWfaNan[4] = {0x50, 0x6F, 0x9A, 0x13};

// Bounded information-element iterator. Stops (returns false) on overrun.
bool next_ie(const uint8_t* p, size_t len, size_t& off, uint8_t& id, const uint8_t*& body,
             uint8_t& blen) {
  if (off + 2 > len) return false;
  id = p[off];
  blen = p[off + 1];
  if (off + 2 + blen > len) return false;
  body = p + off + 2;
  off += 2 + (size_t)blen;
  return true;
}

void copy_ssid(char* dst, const uint8_t* s, uint8_t n) {
  if (n > 32) n = 32;
  for (uint8_t i = 0; i < n; i++) dst[i] = (s[i] >= 0x20 && s[i] <= 0x7E) ? (char)s[i] : '?';
  dst[n] = '\0';
}

void parse_beacon_ies(const uint8_t* f, size_t len, WifiParse& out) {
  size_t off = kHdrLen + kBeaconFixed;
  uint8_t id, blen;
  const uint8_t* body;
  bool have_ssid = false, dji = false;
  while (next_ie(f, len, off, id, body, blen)) {
    if (id == 0x00) {
      copy_ssid(out.ssid, body, blen);
      have_ssid = true;
    } else if (id == 0xDD) {
      if (blen > 5 && memcmp(body, kOdidOui, 4) == 0 && !out.odid) {
        out.odid = body + 5;  // skip OUI(3) + type(1) + counter(1)
        out.odid_len = blen - 5;
      } else if (blen >= 3 && memcmp(body, kDjiOui, 3) == 0) {
        dji = true;
      }
    }
  }
  if (out.odid) out.kind = WifiKind::ODID_BEACON;
  else if (dji) out.kind = WifiKind::DJI_LEGACY;
  else if (have_ssid) out.kind = WifiKind::PLAIN_BEACON;
}

void parse_nan(const uint8_t* f, size_t len, WifiParse& out) {
  if (memcmp(f + 4, kNanDst, 6) != 0) return;
  size_t off = kHdrLen;
  if (off + 6 > len) return;
  if (f[off] != 0x04 || f[off + 1] != 0x09 || memcmp(f + off + 2, kWfaNan, 4) != 0) return;
  off += 6;
  // NAN attributes: id(1) len(2 LE) body
  while (off + 3 <= len) {
    uint8_t aid = f[off];
    size_t alen = (size_t)f[off + 1] | ((size_t)f[off + 2] << 8);
    size_t body = off + 3;
    if (body + alen > len) return;
    off = body + alen;
    if (aid != 0x03 || alen < 9) continue;  // Service Descriptor Attribute
    if (memcmp(f + body, kNanOdidServiceId, 6) != 0) continue;
    uint8_t control = f[body + 8];
    if (!(control & 0x10) || alen < 10) continue;  // service info present
    size_t si_len = f[body + 9];
    size_t si = body + 10;
    if (si_len < 2 || si + si_len > body + alen) continue;
    out.odid = f + si + 1;  // skip message counter
    out.odid_len = si_len - 1;
    out.kind = WifiKind::ODID_NAN;
    return;
  }
}
}  // namespace

bool wifi_parse_mgmt(const uint8_t* frame, size_t len, WifiParse& out) {
  memset(&out, 0, sizeof out);
  if (!frame || len < kHdrLen) return false;
  uint8_t fc0 = frame[0];
  if (fc0 != 0x80 && fc0 != 0x50 && fc0 != 0xD0) return false;
  memcpy(out.src, frame + 10, 6);
  if (fc0 == 0xD0) parse_nan(frame, len, out);
  else if (len >= kHdrLen + kBeaconFixed) parse_beacon_ies(frame, len, out);
  return true;
}
