#include <unity.h>
#include <cstring>
#include "detect/lora_headers.h"
#include "helpers/odid_encode.h"
void setUp() {}
void tearDown() {}

static void test_mesh_header_fields() {
  uint8_t p[20] = {0xFF, 0xFF, 0xFF, 0xFF, 0xD4, 0xC3, 0xB2, 0xA1, 0x78, 0x56, 0x34, 0x12, 0x63, 0x08, 0, 0};
  MeshHeader h;
  TEST_ASSERT_TRUE(parse_meshtastic(p, sizeof p, h));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, h.dest);
  TEST_ASSERT_EQUAL_HEX32(0xA1B2C3D4, h.sender);
  TEST_ASSERT_EQUAL_HEX32(0x12345678, h.packet_id);
  TEST_ASSERT_EQUAL(3, h.hop_limit);
  TEST_ASSERT_EQUAL(3, h.hop_start);
  TEST_ASSERT_FALSE(h.want_ack);
  TEST_ASSERT_EQUAL_HEX8(0x08, h.channel_hash);
  char id[24];
  format_mesh_id(h.sender, id);
  TEST_ASSERT_EQUAL_STRING("!a1b2c3d4", id);
}
static void test_mesh_too_short() {
  uint8_t p[15] = {0};
  MeshHeader h;
  TEST_ASSERT_FALSE(parse_meshtastic(p, sizeof p, h));
}
static void test_lw_unconf_up() {
  uint8_t p[16] = {0x40, 0x04, 0x03, 0x02, 0x01, 0x80, 0x2A, 0x00, 0x01, 0xAA, 0xBB, 0xCC, 0xDD, 0, 0, 0};
  LwHeader h;
  TEST_ASSERT_TRUE(parse_lorawan(p, sizeof p, h));
  TEST_ASSERT_EQUAL((int)LwType::UNCONF_UP, (int)h.type);
  TEST_ASSERT_EQUAL_HEX32(0x01020304, h.dev_addr);
  TEST_ASSERT_EQUAL(42, h.fcnt);
  char id[24];
  format_lw_id(h, id);
  TEST_ASSERT_EQUAL_STRING("DA:01020304", id);
}
static void test_lw_join_request() {
  uint8_t p[23] = {0x00, 1, 2, 3, 4, 5, 6, 7, 8, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0, 0, 0, 0, 0, 0};
  LwHeader h;
  TEST_ASSERT_TRUE(parse_lorawan(p, sizeof p, h));
  TEST_ASSERT_EQUAL((int)LwType::JOIN_REQUEST, (int)h.type);
  TEST_ASSERT_TRUE(h.join_eui == 0x0807060504030201ULL);
  TEST_ASSERT_TRUE(h.dev_eui == 0x8877665544332211ULL);
  char id[24];
  format_lw_id(h, id);
  TEST_ASSERT_EQUAL_STRING("EUI:8877665544332211", id);
}
static void test_lw_join_request_wrong_len() {
  uint8_t p[22] = {0x00};
  LwHeader h;
  TEST_ASSERT_FALSE(parse_lorawan(p, sizeof p, h));
}
static void test_lw_too_short() {
  uint8_t p[11] = {0x40};
  LwHeader h;
  TEST_ASSERT_FALSE(parse_lorawan(p, sizeof p, h));
}
static void test_lw_bad_major_rejected() {
  uint8_t p[16] = {0x41, 0x04, 0x03, 0x02, 0x01};
  LwHeader h;
  TEST_ASSERT_FALSE(parse_lorawan(p, sizeof p, h));
}
static void test_lora_fuzz_no_crash() {
  Lcg r(11);
  uint8_t buf[64];
  for (int it = 0; it < 10000; it++) {
    size_t len = r.next() % 65;
    for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)r.next();
    MeshHeader m;
    LwHeader l;
    parse_meshtastic(buf, len, m);
    parse_lorawan(buf, len, l);
  }
  for (size_t k = 0; k <= 23; k++) {
    uint8_t* p = new uint8_t[k ? k : 1];
    memset(p, 0x40, k);
    MeshHeader m;
    LwHeader l;
    parse_meshtastic(p, k, m);
    parse_lorawan(p, k, l);
    delete[] p;
  }
  LwHeader z;
  TEST_ASSERT_FALSE(parse_lorawan(nullptr, 0, z));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_mesh_header_fields);
  RUN_TEST(test_mesh_too_short);
  RUN_TEST(test_lw_unconf_up);
  RUN_TEST(test_lw_join_request);
  RUN_TEST(test_lw_join_request_wrong_len);
  RUN_TEST(test_lw_too_short);
  RUN_TEST(test_lw_bad_major_rejected);
  RUN_TEST(test_lora_fuzz_no_crash);
  return UNITY_END();
}
