#include "selftest/vectors.h"
#include <cstring>
#include "detect/wifi_frames.h"
#include "selftest/odid_encode.h"

const uint8_t kVecWifiMac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x03};
const uint8_t kVecTelloMac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
const uint8_t kVecBleMac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};

namespace {
struct W {
  Vec& v;
  void put(uint8_t b) { if (v.len < sizeof v.data) v.data[v.len++] = b; }
  void put(const uint8_t* p, size_t n) { for (size_t i = 0; i < n; i++) put(p[i]); }
};

void mgmt_header(W& w, uint8_t fc0, const uint8_t* dst, const uint8_t* src) {
  w.put(fc0); w.put(0); w.put(0); w.put(0);
  w.put(dst, 6); w.put(src, 6); w.put(src, 6);
  w.put(0x10); w.put(0);
}

const uint8_t kBcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

size_t rid_pack(uint8_t* pack) {
  uint8_t msgs[75];
  enc_basic(msgs, 1, 2, kVecSerialWifi);
  enc_location(msgs + 25, kVecDroneLat, kVecDroneLon, 120.0f, 40.0f, 8.0f, 45.0f);
  enc_system(msgs + 50, kVecPilotLat, kVecPilotLon, 95.0f);
  return enc_pack(pack, msgs, 3);
}

void beacon_fixed(W& w) { for (int i = 0; i < 12; i++) w.put(0); }
void ssid_ie(W& w, const char* s) { w.put(0); w.put((uint8_t)strlen(s)); w.put((const uint8_t*)s, strlen(s)); }

void ble_legacy(Vec& v, const uint8_t* msg) {
  W w{v};
  const uint8_t head[] = {0x02, 0x01, 0x06, 0x1E, 0x16, 0xFA, 0xFF, 0x0D, 0x00};
  w.put(head, sizeof head);
  w.put(msg, 25);
}
}  // namespace

void vec_rid_beacon(Vec& v) {
  v = Vec{"rid_beacon", 0, {}};
  W w{v};
  mgmt_header(w, 0x80, kBcast, kVecWifiMac);
  beacon_fixed(w);
  ssid_ie(w, "RID-SELFTEST");
  uint8_t pack[3 + 75];
  size_t n = rid_pack(pack);
  w.put(0xDD); w.put((uint8_t)(5 + n));
  const uint8_t oui[5] = {0xFA, 0x0B, 0xBC, 0x0D, 0x01};
  w.put(oui, 5);
  w.put(pack, n);
}

void vec_rid_nan(Vec& v) {
  v = Vec{"rid_nan", 0, {}};
  W w{v};
  const uint8_t nan_dst[6] = {0x51, 0x6F, 0x9A, 0x01, 0x00, 0x00};
  mgmt_header(w, 0xD0, nan_dst, kVecWifiMac);
  const uint8_t sdf[6] = {0x04, 0x09, 0x50, 0x6F, 0x9A, 0x13};
  w.put(sdf, 6);
  uint8_t pack[3 + 75];
  size_t n = rid_pack(pack);
  uint16_t alen = (uint16_t)(6 + 3 + 1 + 1 + n);
  w.put(0x03); w.put(alen & 0xFF); w.put(alen >> 8);
  w.put(kNanOdidServiceId, 6);
  w.put(0x01); w.put(0x00); w.put(0x10);  // instance, requestor, control: service info present
  w.put((uint8_t)(1 + n));
  w.put(0x02);  // message counter
  w.put(pack, n);
}

void vec_rid_ble_basic(Vec& v) {
  v = Vec{"rid_ble_basic", 0, {}};
  uint8_t m[25];
  enc_basic(m, 1, 2, kVecSerialBle);
  ble_legacy(v, m);
}

void vec_rid_ble_location(Vec& v) {
  v = Vec{"rid_ble_location", 0, {}};
  uint8_t m[25];
  enc_location(m, kVecBleLat, kVecBleLon, 80.0f, 30.0f, 4.0f, 180.0f);
  ble_legacy(v, m);
}

void vec_tello_beacon(Vec& v) {
  v = Vec{"tello_beacon", 0, {}};
  W w{v};
  mgmt_header(w, 0x80, kBcast, kVecTelloMac);
  beacon_fixed(w);
  ssid_ie(w, "TELLO-SELFTEST");
}

void vec_mesh_packet(Vec& v) {
  v = Vec{"mesh_packet", 0, {}};
  W w{v};
  const uint8_t p[24] = {0xFF, 0xFF, 0xFF, 0xFF, 0x57, 0x7E, 0x1F, 0x5E, 0x11, 0x22, 0x33, 0x44,
                         0x63, 0x08, 0x00, 0x00, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07, 0x18};
  w.put(p, sizeof p);
}

void vec_lorawan_uplink(Vec& v) {
  v = Vec{"lorawan_uplink", 0, {}};
  W w{v};
  const uint8_t p[16] = {0x40, 0xDA, 0x1B, 0x01, 0x26, 0x80, 0x07, 0x00,
                         0x01, 0x5A, 0x5A, 0x5A, 0x11, 0x22, 0x33, 0x44};
  w.put(p, sizeof p);
}

size_t vec_elrs_hits(CadHit* out, size_t max, uint32_t t0) {
  const uint32_t ch[4] = {903500, 910100, 915500, 922100};
  size_t n = 0;
  for (size_t i = 0; i < 6 && n < max; i++)
    out[n++] = {ch[i % 4], 7, 500000, t0 + (uint32_t)i * 100, (int16_t)(-80 - (int)i)};
  return n;
}
