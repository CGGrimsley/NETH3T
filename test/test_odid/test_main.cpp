#include <unity.h>
#include <cstring>
#include "detect/odid.h"
#include "helpers/odid_encode.h"

static OdidFields f;
void setUp() { memset(&f, 0, sizeof f); }
void tearDown() {}

static void test_basic_id() {
  uint8_t m[25];
  enc_basic(m, 1, 2, "1581F5FKD229400H0Z0E");
  TEST_ASSERT_EQUAL((int)OdidErr::OK, (int)odid_decode(m, 25, f));
  TEST_ASSERT_TRUE(f.has_basic);
  TEST_ASSERT_EQUAL(1, f.id_type);
  TEST_ASSERT_EQUAL(2, f.ua_type);
  TEST_ASSERT_EQUAL_STRING("1581F5FKD229400H0Z0E", f.uas_id);
}

static void test_location_values() {
  uint8_t m[25];
  enc_location(m, 37.7749, -122.4194, 120.5f, 50.0f, 12.25f, 270.0f);
  TEST_ASSERT_EQUAL((int)OdidErr::OK, (int)odid_decode(m, 25, f));
  TEST_ASSERT_TRUE(f.has_loc);
  TEST_ASSERT_TRUE(f.loc_valid);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 37.7749, f.lat);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, -122.4194, f.lon);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 120.5f, f.alt_geo_m);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 50.0f, f.height_m);
  TEST_ASSERT_FLOAT_WITHIN(0.25f, 12.25f, f.speed_h_ms);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 270.0f, f.direction_deg);
}

static void test_location_high_speed_multiplier() {
  uint8_t m[25];
  enc_location(m, 1, 1, 0, 0, 90.0f, 10.0f);
  odid_decode(m, 25, f);
  TEST_ASSERT_FLOAT_WITHIN(0.75f, 90.0f, f.speed_h_ms);
}

static void test_location_zero_is_invalid() {
  uint8_t m[25];
  enc_location(m, 0.0, 0.0, 10, 10, 0, 0);
  TEST_ASSERT_EQUAL((int)OdidErr::OK, (int)odid_decode(m, 25, f));
  TEST_ASSERT_TRUE(f.has_loc);
  TEST_ASSERT_FALSE(f.loc_valid);
}

static void test_location_out_of_range_invalid() {
  uint8_t m[25];
  enc_location_raw_lat(m, 1000000000, 10);
  odid_decode(m, 25, f);
  TEST_ASSERT_TRUE(f.has_loc);
  TEST_ASSERT_FALSE(f.loc_valid);
}

static void test_system_operator_location() {
  uint8_t m[25];
  enc_system(m, 51.5007, -0.1246, 30.0f);
  TEST_ASSERT_EQUAL((int)OdidErr::OK, (int)odid_decode(m, 25, f));
  TEST_ASSERT_TRUE(f.has_sys);
  TEST_ASSERT_TRUE(f.op_valid);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 51.5007, f.op_lat);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, -0.1246, f.op_lon);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 30.0f, f.op_alt_m);
}

static void test_operator_id() {
  uint8_t m[25];
  enc_operator(m, "FIN87astrdge12k8");
  TEST_ASSERT_EQUAL((int)OdidErr::OK, (int)odid_decode(m, 25, f));
  TEST_ASSERT_TRUE(f.has_opid);
  TEST_ASSERT_EQUAL_STRING("FIN87astrdge12k8", f.operator_id);
}

static void test_self_id_text() {
  uint8_t m[25];
  enc_self_id(m, "Survey flight");
  odid_decode(m, 25, f);
  TEST_ASSERT_TRUE(f.has_self);
  TEST_ASSERT_EQUAL_STRING("Survey flight", f.self_id);
}

static void test_nonprintable_chars_sanitized() {
  uint8_t m[25];
  enc_basic(m, 1, 2, "AB");
  m[3] = 0x07;  // bell
  odid_decode(m, 25, f);
  TEST_ASSERT_EQUAL_STRING("A?", f.uas_id);
}

static void test_auth_flagged() {
  uint8_t m[25] = {0};
  m[0] = (0x2 << 4) | 2;
  TEST_ASSERT_EQUAL((int)OdidErr::OK, (int)odid_decode(m, 25, f));
  TEST_ASSERT_TRUE(f.auth_present);
}

static void build_pack3(uint8_t* pack, size_t* len) {
  uint8_t msgs[75];
  enc_basic(msgs, 1, 2, "SN123");
  enc_location(msgs + 25, 10.0, 20.0, 100, 30, 5, 90);
  enc_system(msgs + 50, 10.001, 20.001, 5);
  *len = enc_pack(pack, msgs, 3);
}

static void test_pack_with_three_messages() {
  uint8_t pack[3 + 75];
  size_t n;
  build_pack3(pack, &n);
  TEST_ASSERT_EQUAL((int)OdidErr::OK, (int)odid_decode(pack, n, f));
  TEST_ASSERT_TRUE(f.has_basic && f.has_loc && f.has_sys);
  TEST_ASSERT_EQUAL_STRING("SN123", f.uas_id);
}

static void test_pack_bad_msg_size() {
  uint8_t pack[3 + 75];
  size_t n;
  build_pack3(pack, &n);
  pack[1] = 24;
  TEST_ASSERT_EQUAL((int)OdidErr::BAD_PACK, (int)odid_decode(pack, n, f));
}

static void test_pack_count_overflow() {
  uint8_t pack[3 + 75];
  size_t n;
  build_pack3(pack, &n);
  pack[2] = 9;
  TEST_ASSERT_EQUAL((int)OdidErr::TOO_SHORT, (int)odid_decode(pack, n, f));
}

static void test_pack_count_zero_bad() {
  uint8_t pack[3 + 75];
  size_t n;
  build_pack3(pack, &n);
  pack[2] = 0;
  TEST_ASSERT_EQUAL((int)OdidErr::BAD_PACK, (int)odid_decode(pack, n, f));
}

static void test_too_short_single() {
  uint8_t m[25];
  enc_basic(m, 1, 2, "X");
  TEST_ASSERT_EQUAL((int)OdidErr::TOO_SHORT, (int)odid_decode(m, 10, f));
  TEST_ASSERT_EQUAL((int)OdidErr::TOO_SHORT, (int)odid_decode(nullptr, 0, f));
}

static void test_unknown_type() {
  uint8_t m[25] = {0};
  m[0] = (0x7 << 4) | 2;
  TEST_ASSERT_EQUAL((int)OdidErr::BAD_TYPE, (int)odid_decode(m, 25, f));
}

static void test_odid_fuzz_no_crash() {
  Lcg r(1);
  uint8_t buf[300];
  for (int it = 0; it < 10000; it++) {
    size_t len = r.next() % 301;
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)r.next();
    if (len && (it & 1)) buf[0] = (uint8_t)((r.next() % 16) << 4 | 2);
    if (len > 2 && (it & 2)) { buf[0] = 0xF2; buf[1] = 25; }
    OdidFields g;
    memset(&g, 0, sizeof g);
    odid_decode(buf, len, g);
  }
  uint8_t pack[3 + 75];
  size_t n;
  build_pack3(pack, &n);
  for (size_t k = 0; k <= n; k++) {
    // copy into exact-size heap buffer so ASan would catch overreads
    uint8_t* p = new uint8_t[k ? k : 1];
    memcpy(p, pack, k);
    OdidFields g;
    memset(&g, 0, sizeof g);
    odid_decode(p, k, g);
    delete[] p;
  }
  TEST_PASS();
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_basic_id);
  RUN_TEST(test_location_values);
  RUN_TEST(test_location_high_speed_multiplier);
  RUN_TEST(test_location_zero_is_invalid);
  RUN_TEST(test_location_out_of_range_invalid);
  RUN_TEST(test_system_operator_location);
  RUN_TEST(test_operator_id);
  RUN_TEST(test_self_id_text);
  RUN_TEST(test_nonprintable_chars_sanitized);
  RUN_TEST(test_auth_flagged);
  RUN_TEST(test_pack_with_three_messages);
  RUN_TEST(test_pack_bad_msg_size);
  RUN_TEST(test_pack_count_overflow);
  RUN_TEST(test_pack_count_zero_bad);
  RUN_TEST(test_too_short_single);
  RUN_TEST(test_unknown_type);
  RUN_TEST(test_odid_fuzz_no_crash);
  return UNITY_END();
}
