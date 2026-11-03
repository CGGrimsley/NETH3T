#include <unity.h>
#include "detect/noise_floor.h"
void setUp() {}
void tearDown() {}

static void test_not_ready_never_hot() {
  NoiseFloor nf;
  TEST_ASSERT_FALSE(nf.ready());
  TEST_ASSERT_FALSE(nf.hot(-20));
}
static void test_p20_of_single_sweep() {
  NoiseFloor nf;
  int8_t s[10] = {-100, -99, -98, -97, -96, -95, -94, -93, -40, -30};
  nf.add_sweep(s, 10);
  TEST_ASSERT_EQUAL(-98, nf.floor_dbm());  // index 2 of 10 sorted
}
static void test_hot_threshold() {
  NoiseFloor nf;
  int8_t s[5] = {-100, -100, -100, -100, -100};
  nf.add_sweep(s, 5);
  TEST_ASSERT_FALSE(nf.hot(-90));
  TEST_ASSERT_TRUE(nf.hot(-89));
}
static void test_strong_signals_do_not_lift_floor() {
  NoiseFloor nf;
  int8_t s[10] = {-100, -100, -100, -100, -100, -100, -100, -50, -50, -50};
  nf.add_sweep(s, 10);
  TEST_ASSERT_EQUAL(-100, nf.floor_dbm());
}
static void test_mean_over_history_and_rollover() {
  NoiseFloor nf;
  int8_t a[4] = {-100, -100, -100, -100};
  int8_t b[4] = {-80, -80, -80, -80};
  nf.add_sweep(a, 4);
  nf.add_sweep(b, 4);
  TEST_ASSERT_EQUAL(-90, nf.floor_dbm());
  for (int i = 0; i < 16; i++) nf.add_sweep(b, 4);
  TEST_ASSERT_EQUAL(-80, nf.floor_dbm());
}
static void test_empty_sweep_ignored() {
  NoiseFloor nf;
  nf.add_sweep(nullptr, 0);
  TEST_ASSERT_FALSE(nf.ready());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_not_ready_never_hot);
  RUN_TEST(test_p20_of_single_sweep);
  RUN_TEST(test_hot_threshold);
  RUN_TEST(test_strong_signals_do_not_lift_floor);
  RUN_TEST(test_mean_over_history_and_rollover);
  RUN_TEST(test_empty_sweep_ignored);
  return UNITY_END();
}
