#include <unity.h>
#include <cstdio>
#include "detect/id_cache.h"
void setUp() {}
void tearDown() {}

static const uint8_t A[6] = {0xAA, 0xBB, 0xCC, 0x01, 0x02, 0x03};
static const uint8_t B[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

static void test_format_mac() {
  char s[18];
  format_mac(A, s);
  TEST_ASSERT_EQUAL_STRING("AA:BB:CC:01:02:03", s);
}
static void test_unknown_mac_resolves_to_mac() {
  IdCache c;
  char id[24];
  TEST_ASSERT_FALSE(c.resolve(A, id));
  TEST_ASSERT_EQUAL_STRING("AA:BB:CC:01:02:03", id);
}
static void test_remembered_serial() {
  IdCache c;
  c.remember(A, "SN-1", 10);
  char id[24];
  TEST_ASSERT_TRUE(c.resolve(A, id));
  TEST_ASSERT_EQUAL_STRING("SN-1", id);
  TEST_ASSERT_FALSE(c.resolve(B, id));
}
static void test_update_serial_same_mac() {
  IdCache c;
  c.remember(A, "SN-1", 10);
  c.remember(A, "SN-2", 20);
  char id[24];
  c.resolve(A, id);
  TEST_ASSERT_EQUAL_STRING("SN-2", id);
}
static void test_evicts_oldest_when_full() {
  IdCache c;
  uint8_t m[6] = {0, 0, 0, 0, 0, 0};
  char sn[24];
  for (int i = 0; i < 33; i++) {
    m[5] = (uint8_t)i;
    snprintf(sn, sizeof sn, "S%d", i);
    c.remember(m, sn, (uint32_t)i);
  }
  char id[24];
  m[5] = 0;
  TEST_ASSERT_FALSE(c.resolve(m, id));
  m[5] = 32;
  TEST_ASSERT_TRUE(c.resolve(m, id));
  TEST_ASSERT_EQUAL_STRING("S32", id);
}
static void test_empty_serial_ignored() {
  IdCache c;
  c.remember(A, "", 10);
  char id[24];
  TEST_ASSERT_FALSE(c.resolve(A, id));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_format_mac);
  RUN_TEST(test_unknown_mac_resolves_to_mac);
  RUN_TEST(test_remembered_serial);
  RUN_TEST(test_update_serial_same_mac);
  RUN_TEST(test_evicts_oldest_when_full);
  RUN_TEST(test_empty_serial_ignored);
  return UNITY_END();
}
