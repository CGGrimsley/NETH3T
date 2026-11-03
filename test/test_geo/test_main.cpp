#include <unity.h>
#include <cstring>
#include "ui/geo.h"
void setUp() {}
void tearDown() {}

static void test_haversine_known() {
  double d = haversine_m(51.5007, -0.1246, 40.6892, -74.0445);
  TEST_ASSERT_DOUBLE_WITHIN(5574840.0 * 0.005, 5574840.0, d);
}
static void test_haversine_zero() { TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, haversine_m(10, 20, 10, 20)); }
static void test_distance_requires_both_fixes() {
  OdidFields f;
  memset(&f, 0, sizeof f);
  f.loc_valid = true; f.lat = 1; f.lon = 1;
  f.op_valid = false;
  double d = -1;
  TEST_ASSERT_FALSE(drone_pilot_distance_m(f, &d));
  f.op_valid = true; f.op_lat = 1; f.op_lon = 1.001;
  TEST_ASSERT_TRUE(drone_pilot_distance_m(f, &d));
  TEST_ASSERT_DOUBLE_WITHIN(1.0, 111.3, d);
  f.loc_valid = false;
  TEST_ASSERT_FALSE(drone_pilot_distance_m(f, &d));
}
static void test_pulse_mapping() {
  TEST_ASSERT_EQUAL(10, rssi_to_pulse_hz_x10(-110));
  TEST_ASSERT_EQUAL(100, rssi_to_pulse_hz_x10(-40));
  TEST_ASSERT_EQUAL(55, rssi_to_pulse_hz_x10(-75));
  TEST_ASSERT_EQUAL(10, rssi_to_pulse_hz_x10(-200));
  TEST_ASSERT_EQUAL(100, rssi_to_pulse_hz_x10(0));
}
static void test_tone_mapping() {
  TEST_ASSERT_EQUAL(400, rssi_to_tone_hz(-110));
  TEST_ASSERT_EQUAL(2000, rssi_to_tone_hz(-40));
  TEST_ASSERT_EQUAL(1200, rssi_to_tone_hz(-75));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_haversine_known);
  RUN_TEST(test_haversine_zero);
  RUN_TEST(test_distance_requires_both_fixes);
  RUN_TEST(test_pulse_mapping);
  RUN_TEST(test_tone_mapping);
  return UNITY_END();
}
