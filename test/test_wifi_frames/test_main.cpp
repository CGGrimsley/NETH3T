#include <unity.h>
#include <cstring>
#include <vector>
#include "detect/wifi_frames.h"
#include "helpers/odid_encode.h"

using Bytes = std::vector<uint8_t>;
static const uint8_t kSrc[6] = {0x60, 0x60, 0x1F, 0x11, 0x22, 0x33};

static void put(Bytes& b, std::initializer_list<uint8_t> v) { b.insert(b.end(), v); }
static void put(Bytes& b, const uint8_t* p, size_t n) { b.insert(b.end(), p, p + n); }

static Bytes mgmt_header(uint8_t fc0, const uint8_t* addr1) {
  Bytes b;
  put(b, {fc0, 0x00, 0x00, 0x00});
  put(b, addr1, 6);
  put(b, kSrc, 6);
  put(b, kSrc, 6);
  put(b, {0x10, 0x00});
  return b;
}
static const uint8_t kBcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static Bytes beacon(uint8_t fc0 = 0x80) {
  Bytes b = mgmt_header(fc0, kBcast);
  for (int i = 0; i < 12; i++) b.push_back(0);  // timestamp, interval, capability
  return b;
}
static void ie_ssid(Bytes& b, const char* s) {
  b.push_back(0x00);
  b.push_back((uint8_t)strlen(s));
  put(b, (const uint8_t*)s, strlen(s));
}
static size_t make_pack2(uint8_t* pack) {
  uint8_t msgs[50];
  enc_basic(msgs, 1, 2, "SN-WIFI");
  enc_location(msgs + 25, 10, 20, 100, 30, 5, 90);
  return enc_pack(pack, msgs, 2);
}
static void ie_odid(Bytes& b) {
  uint8_t pack[53];
  size_t n = make_pack2(pack);
  b.push_back(0xDD);
  b.push_back((uint8_t)(5 + n));
  put(b, {0xFA, 0x0B, 0xBC, 0x0D, 0x07});
  put(b, pack, n);
}

static Bytes nan_frame(const uint8_t* service_id) {
  static const uint8_t kNanDst[6] = {0x51, 0x6F, 0x9A, 0x01, 0x00, 0x00};
  Bytes b = mgmt_header(0xD0, kNanDst);
  put(b, {0x04, 0x09, 0x50, 0x6F, 0x9A, 0x13});
  uint8_t pack[53];
  size_t n = make_pack2(pack);
  uint16_t alen = (uint16_t)(6 + 3 + 1 + 1 + n);
  put(b, {0x03, (uint8_t)(alen & 0xFF), (uint8_t)(alen >> 8)});
  put(b, service_id, 6);
  put(b, {0x01, 0x00, 0x10});  // instance, requestor, control (service info present)
  b.push_back((uint8_t)(1 + n));
  b.push_back(0x05);           // message counter
  put(b, pack, n);
  return b;
}

static WifiParse w;
void setUp() { memset(&w, 0, sizeof w); }
void tearDown() {}

static void test_service_id_constant() {
  const uint8_t expect[6] = {0x88, 0x69, 0x19, 0x9D, 0x92, 0x09};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expect, kNanOdidServiceId, 6);
}

static void test_beacon_with_odid_ie() {
  Bytes b = beacon();
  ie_ssid(b, "RID-1");
  ie_odid(b);
  TEST_ASSERT_TRUE(wifi_parse_mgmt(b.data(), b.size(), w));
  TEST_ASSERT_EQUAL((int)WifiKind::ODID_BEACON, (int)w.kind);
  TEST_ASSERT_EQUAL(3 + 2 * 25, w.odid_len);
  TEST_ASSERT_EQUAL_HEX8(0xF2, w.odid[0]);
  TEST_ASSERT_EQUAL_STRING("RID-1", w.ssid);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(kSrc, w.src, 6);
}

static void test_beacon_plain_ssid() {
  Bytes b = beacon();
  ie_ssid(b, "TELLO-5F3A");
  TEST_ASSERT_TRUE(wifi_parse_mgmt(b.data(), b.size(), w));
  TEST_ASSERT_EQUAL((int)WifiKind::PLAIN_BEACON, (int)w.kind);
  TEST_ASSERT_EQUAL_STRING("TELLO-5F3A", w.ssid);
  TEST_ASSERT_NULL(w.odid);
}

static void test_probe_response_treated_like_beacon() {
  Bytes b = beacon(0x50);
  ie_ssid(b, "Anafi-1");
  TEST_ASSERT_TRUE(wifi_parse_mgmt(b.data(), b.size(), w));
  TEST_ASSERT_EQUAL((int)WifiKind::PLAIN_BEACON, (int)w.kind);
}

static void test_nan_sdf_with_odid() {
  Bytes b = nan_frame(kNanOdidServiceId);
  TEST_ASSERT_TRUE(wifi_parse_mgmt(b.data(), b.size(), w));
  TEST_ASSERT_EQUAL((int)WifiKind::ODID_NAN, (int)w.kind);
  TEST_ASSERT_EQUAL(53, w.odid_len);
  TEST_ASSERT_EQUAL_HEX8(0xF2, w.odid[0]);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(kSrc, w.src, 6);
}

static void test_nan_wrong_service_id_ignored() {
  const uint8_t other[6] = {1, 2, 3, 4, 5, 6};
  Bytes b = nan_frame(other);
  wifi_parse_mgmt(b.data(), b.size(), w);
  TEST_ASSERT_EQUAL((int)WifiKind::NONE, (int)w.kind);
}

static void test_dji_legacy_ie() {
  Bytes b = beacon();
  ie_ssid(b, "x");
  put(b, {0xDD, 0x06, 0x26, 0x37, 0x12, 0x58, 0x62, 0x13});
  TEST_ASSERT_TRUE(wifi_parse_mgmt(b.data(), b.size(), w));
  TEST_ASSERT_EQUAL((int)WifiKind::DJI_LEGACY, (int)w.kind);
  TEST_ASSERT_NULL(w.odid);
}

static void test_ie_length_overruns_frame() {
  Bytes b = beacon();
  put(b, {0xDD, 0xF0, 0xFA, 0x0B, 0xBC, 0x0D});  // claims 240 bytes
  uint8_t* p = new uint8_t[b.size()];
  memcpy(p, b.data(), b.size());
  bool ok = wifi_parse_mgmt(p, b.size(), w);
  delete[] p;
  TEST_ASSERT_TRUE(!ok || w.kind == WifiKind::NONE);
  TEST_ASSERT_NULL(w.odid);
}

static void test_non_mgmt_frame_rejected() {
  Bytes b = beacon(0x08);
  TEST_ASSERT_FALSE(wifi_parse_mgmt(b.data(), b.size(), w));
}

static void test_short_frame_rejected() {
  uint8_t b[10] = {0x80};
  TEST_ASSERT_FALSE(wifi_parse_mgmt(b, sizeof b, w));
}

static void test_wifi_fuzz_no_crash() {
  Lcg r(7);
  uint8_t buf[400];
  for (int it = 0; it < 10000; it++) {
    size_t len = r.next() % 401;
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)r.next();
    if (len) buf[0] = (uint8_t[]){0x80, 0x50, 0xD0}[it % 3];
    WifiParse g;
    wifi_parse_mgmt(buf, len, g);
  }
  Bytes n = nan_frame(kNanOdidServiceId);
  Bytes bc = beacon();
  ie_ssid(bc, "abc");
  ie_odid(bc);
  for (const Bytes* f : {&n, &bc}) {
    for (size_t k = 0; k <= f->size(); k++) {
      uint8_t* p = new uint8_t[k ? k : 1];
      memcpy(p, f->data(), k);
      WifiParse g;
      if (wifi_parse_mgmt(p, k, g) && g.odid) {
        TEST_ASSERT_TRUE(g.odid >= p && g.odid + g.odid_len <= p + k);
      }
      delete[] p;
    }
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_service_id_constant);
  RUN_TEST(test_beacon_with_odid_ie);
  RUN_TEST(test_beacon_plain_ssid);
  RUN_TEST(test_probe_response_treated_like_beacon);
  RUN_TEST(test_nan_sdf_with_odid);
  RUN_TEST(test_nan_wrong_service_id_ignored);
  RUN_TEST(test_dji_legacy_ie);
  RUN_TEST(test_ie_length_overruns_frame);
  RUN_TEST(test_non_mgmt_frame_rejected);
  RUN_TEST(test_short_frame_rejected);
  RUN_TEST(test_wifi_fuzz_no_crash);
  return UNITY_END();
}
