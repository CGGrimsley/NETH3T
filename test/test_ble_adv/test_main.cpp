#include <unity.h>
#include <cstring>
#include <vector>
#include "detect/ble_adv.h"
#include "helpers/odid_encode.h"

using Bytes = std::vector<uint8_t>;
void setUp() {}
void tearDown() {}

static Bytes adv_with(const uint8_t* payload, size_t n, uint16_t uuid = 0xFFFA, uint8_t app = 0x0D) {
  Bytes b = {0x02, 0x01, 0x06};  // flags AD
  b.push_back((uint8_t)(1 + 2 + 1 + 1 + n));
  b.push_back(0x16);
  b.push_back(uuid & 0xFF);
  b.push_back(uuid >> 8);
  b.push_back(app);
  b.push_back(0x2A);  // counter
  b.insert(b.end(), payload, payload + n);
  return b;
}

static void test_legacy_basic_id() {
  uint8_t m[25];
  enc_basic(m, 1, 2, "SN-BLE");
  Bytes b = adv_with(m, 25);
  TEST_ASSERT_EQUAL_HEX8(0x1E, b[3]);
  const uint8_t* o = nullptr;
  size_t n = 0;
  TEST_ASSERT_TRUE(ble_find_odid(b.data(), b.size(), &o, &n));
  TEST_ASSERT_EQUAL(25, n);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(m, o, 25);
}

static void test_extended_pack() {
  uint8_t msgs[50], pack[53];
  enc_basic(msgs, 1, 2, "SN");
  enc_operator(msgs + 25, "OP");
  size_t pn = enc_pack(pack, msgs, 2);
  Bytes b = adv_with(pack, pn);
  const uint8_t* o;
  size_t n;
  TEST_ASSERT_TRUE(ble_find_odid(b.data(), b.size(), &o, &n));
  TEST_ASSERT_EQUAL(3 + 50, n);
}

static void test_wrong_uuid() {
  uint8_t m[25] = {0};
  Bytes b = adv_with(m, 25, 0xFFFB);
  const uint8_t* o;
  size_t n;
  TEST_ASSERT_FALSE(ble_find_odid(b.data(), b.size(), &o, &n));
}

static void test_wrong_app_code() {
  uint8_t m[25] = {0};
  Bytes b = adv_with(m, 25, 0xFFFA, 0x0C);
  const uint8_t* o;
  size_t n;
  TEST_ASSERT_FALSE(ble_find_odid(b.data(), b.size(), &o, &n));
}

static void test_zero_length_ad_terminates() {
  uint8_t b[] = {0x02, 0x01, 0x06, 0x00, 0x16, 0xFA, 0xFF, 0x0D};
  const uint8_t* o;
  size_t n;
  TEST_ASSERT_FALSE(ble_find_odid(b, sizeof b, &o, &n));
}

static void test_ble_fuzz_no_crash() {
  Lcg r(3);
  uint8_t buf[255];
  for (int it = 0; it < 10000; it++) {
    size_t len = r.next() % 256;
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)r.next();
    if (len > 4 && (it & 1)) { buf[1] = 0x16; buf[2] = 0xFA; buf[3] = 0xFF; buf[4] = 0x0D; }
    const uint8_t* o;
    size_t n;
    if (ble_find_odid(buf, len, &o, &n)) TEST_ASSERT_TRUE(o >= buf && o + n <= buf + len);
  }
  uint8_t m[25];
  enc_basic(m, 1, 2, "SN");
  Bytes b = adv_with(m, 25);
  for (size_t k = 0; k <= b.size(); k++) {
    uint8_t* p = new uint8_t[k ? k : 1];
    memcpy(p, b.data(), k);
    const uint8_t* o;
    size_t n;
    ble_find_odid(p, k, &o, &n);
    delete[] p;
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_legacy_basic_id);
  RUN_TEST(test_extended_pack);
  RUN_TEST(test_wrong_uuid);
  RUN_TEST(test_wrong_app_code);
  RUN_TEST(test_zero_length_ad_terminates);
  RUN_TEST(test_ble_fuzz_no_crash);
  return UNITY_END();
}
