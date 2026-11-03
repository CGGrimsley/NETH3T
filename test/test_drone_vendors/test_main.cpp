#include <unity.h>
#include "detect/drone_vendors.h"
void setUp() {}
void tearDown() {}

static void test_dji_oui() {
  const uint8_t m[6] = {0x60, 0x60, 0x1F, 1, 2, 3};
  TEST_ASSERT_EQUAL_STRING("DJI", drone_vendor_by_oui(m));
}
static void test_dji_last_entry_oui() {
  const uint8_t m[6] = {0xEC, 0x72, 0xF7, 1, 2, 3};
  TEST_ASSERT_EQUAL_STRING("DJI", drone_vendor_by_oui(m));
  const uint8_t m2[6] = {0x04, 0xA8, 0x5A, 0, 0, 0};
  TEST_ASSERT_EQUAL_STRING("DJI", drone_vendor_by_oui(m2));
}
static void test_parrot_oui() {
  const uint8_t m[6] = {0x90, 0x03, 0xB7, 1, 2, 3};
  TEST_ASSERT_EQUAL_STRING("Parrot", drone_vendor_by_oui(m));
}
static void test_skydio_oui() {
  const uint8_t m[6] = {0x38, 0x1D, 0x14, 1, 2, 3};
  TEST_ASSERT_EQUAL_STRING("Skydio", drone_vendor_by_oui(m));
}
static void test_router_oui_no_match() {
  const uint8_t m[6] = {0x50, 0xC7, 0xBF, 1, 2, 3};
  TEST_ASSERT_NULL(drone_vendor_by_oui(m));
}
static void test_ssid_tello() { TEST_ASSERT_EQUAL_STRING("Ryze/DJI Tello", drone_vendor_by_ssid("tello-5F3A")); }
static void test_ssid_anafi() { TEST_ASSERT_EQUAL_STRING("Parrot", drone_vendor_by_ssid("Anafi-123456")); }
static void test_ssid_mavic_contains() { TEST_ASSERT_EQUAL_STRING("DJI", drone_vendor_by_ssid("My Mavic Air")); }
static void test_ssid_evo() {
  TEST_ASSERT_EQUAL_STRING("Autel", drone_vendor_by_ssid("EVO_123"));
  TEST_ASSERT_EQUAL_STRING("Autel", drone_vendor_by_ssid("EVOII-8K"));
}
static void test_ssid_no_false_positive() {
  TEST_ASSERT_NULL(drone_vendor_by_ssid("Evolution_Home"));
  TEST_ASSERT_NULL(drone_vendor_by_ssid("HomeWiFi"));
  TEST_ASSERT_NULL(drone_vendor_by_ssid("DIRECT-xy"));
  TEST_ASSERT_NULL(drone_vendor_by_ssid("Everest"));
}
static void test_ssid_empty_or_null() {
  TEST_ASSERT_NULL(drone_vendor_by_ssid(""));
  TEST_ASSERT_NULL(drone_vendor_by_ssid(nullptr));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_dji_oui);
  RUN_TEST(test_dji_last_entry_oui);
  RUN_TEST(test_parrot_oui);
  RUN_TEST(test_skydio_oui);
  RUN_TEST(test_router_oui_no_match);
  RUN_TEST(test_ssid_tello);
  RUN_TEST(test_ssid_anafi);
  RUN_TEST(test_ssid_mavic_contains);
  RUN_TEST(test_ssid_evo);
  RUN_TEST(test_ssid_no_false_positive);
  RUN_TEST(test_ssid_empty_or_null);
  return UNITY_END();
}
