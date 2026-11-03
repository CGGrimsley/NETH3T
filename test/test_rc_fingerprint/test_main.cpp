#include <unity.h>
#include <cstring>
#include "detect/rc_fingerprint.h"

static RcFingerprint* fp;
static Observation out[8];
void setUp() { fp = new RcFingerprint(); memset(out, 0, sizeof out); }
void tearDown() { delete fp; }

static void cad(uint32_t f, uint8_t sf, uint32_t bw, uint32_t t, int16_t rssi = -90) {
  fp->add_cad({f, sf, bw, t, rssi});
}

static void test_elrs900_detected() {
  const uint32_t ch[4] = {903500, 910100, 915500, 922100};
  for (int i = 0; i < 6; i++) cad(ch[i % 4], 7, 500000, 1000 + i * 250);
  size_t n = fp->evaluate(2500, out, 8);
  TEST_ASSERT_EQUAL(1, n);
  TEST_ASSERT_EQUAL((int)SourceType::ELRS, (int)out[0].type);
  TEST_ASSERT_EQUAL_STRING("ELRS900-SF7", out[0].id);
  TEST_ASSERT_EQUAL_STRING("ELRS 915 ~100Hz", out[0].label);
  TEST_ASSERT_EQUAL(100, out[0].lora.rate_hz);
  TEST_ASSERT_EQUAL(7, out[0].lora.sf);
  TEST_ASSERT_EQUAL((int)Confidence::FINGERPRINT, (int)out[0].conf);
  TEST_ASSERT_EQUAL_UINT32(2500, out[0].t_ms);
}
static void test_elrs868_label() {
  const uint32_t ch[3] = {863275, 865375, 869575};
  for (int i = 0; i < 5; i++) cad(ch[i % 3], 8, 500000, 100 + i * 100);
  size_t n = fp->evaluate(700, out, 8);
  TEST_ASSERT_EQUAL(1, n);
  TEST_ASSERT_EQUAL_STRING("ELRS 868 ~50Hz", out[0].label);
}
static void test_elrs24_detected() {
  const uint32_t ch[5] = {2400400, 2420400, 2440400, 2460400, 2479400};
  for (int i = 0; i < 5; i++) cad(ch[i], 6, 812500, 100 + i * 50);
  size_t n = fp->evaluate(500, out, 8);
  TEST_ASSERT_EQUAL(1, n);
  TEST_ASSERT_EQUAL_STRING("ELRS24-SF6", out[0].id);
  TEST_ASSERT_EQUAL(250, out[0].lora.rate_hz);
}
static void test_too_few_channels_no_elrs() {
  for (int i = 0; i < 6; i++) cad(i % 2 ? 903500 : 904100, 7, 500000, 100 + i * 100);
  TEST_ASSERT_EQUAL(0, fp->evaluate(800, out, 8));
}
static void test_mixed_sf_not_combined() {
  const uint32_t ch[4] = {903500, 910100, 915500, 922100};
  for (int i = 0; i < 4; i++) cad(ch[i], (uint8_t)(6 + (i % 2)), 500000, 100 + i * 100);
  TEST_ASSERT_EQUAL(0, fp->evaluate(600, out, 8));
}
static void test_hits_outside_window_ignored() {
  const uint32_t ch[6] = {903500, 906500, 910100, 915500, 918500, 922100};
  for (int i = 0; i < 6; i++) cad(ch[i], 7, 500000, (uint32_t)i * 1000);
  TEST_ASSERT_EQUAL(0, fp->evaluate(5000, out, 8));
}
static void test_fhss_detected() {
  for (int i = 0; i < 8; i++) fp->add_hot({(uint32_t)(903000 + i * 3000), (uint32_t)(100 + i * 100), (int16_t)(-80 + i)});
  size_t n = fp->evaluate(1000, out, 8);
  TEST_ASSERT_EQUAL(1, n);
  TEST_ASSERT_EQUAL((int)SourceType::FHSS, (int)out[0].type);
  TEST_ASSERT_EQUAL_STRING("FHSS915", out[0].id);
  TEST_ASSERT_EQUAL((int)Confidence::HEURISTIC, (int)out[0].conf);
}
static void test_static_carrier_not_fhss() {
  for (int i = 0; i < 9; i++) fp->add_hot({915000, (uint32_t)(100 + i * 50), -70});
  fp->add_hot({920000, 600, -70});
  TEST_ASSERT_EQUAL(0, fp->evaluate(1000, out, 8));
}
static void test_elrs_suppresses_fhss_same_band() {
  const uint32_t ch[4] = {903500, 910100, 915500, 922100};
  for (int i = 0; i < 6; i++) cad(ch[i % 4], 7, 500000, 1000 + i * 100);
  for (int i = 0; i < 8; i++) fp->add_hot({(uint32_t)(903000 + i * 3000), (uint32_t)(1000 + i * 100), -80});
  size_t n = fp->evaluate(2000, out, 8);
  TEST_ASSERT_EQUAL(1, n);
  TEST_ASSERT_EQUAL((int)SourceType::ELRS, (int)out[0].type);
}
static void test_rssi_is_max_of_window() {
  const uint32_t ch[4] = {903500, 910100, 915500, 922100};
  const int16_t r[4] = {-95, -60, -88, -90};
  for (int i = 0; i < 4; i++) cad(ch[i], 7, 500000, 100 + i * 100, r[i]);
  fp->evaluate(600, out, 8);
  TEST_ASSERT_EQUAL(-60, out[0].rssi_dbm);
}
static void test_recent_cad_returns_window() {
  cad(903500, 7, 500000, 100);
  cad(910100, 7, 500000, 3000);
  CadHit h[4];
  TEST_ASSERT_EQUAL(1, fp->recent_cad(3500, h, 4));
  TEST_ASSERT_EQUAL_UINT32(910100, h[0].freq_khz);
}
static void test_ring_overflow_safe() {
  for (int i = 0; i < 1000; i++) cad((uint32_t)(903500 + (i % 40) * 600), 7, 500000, (uint32_t)i);
  for (int i = 0; i < 1000; i++) fp->add_hot({(uint32_t)(903000 + (i % 20) * 1000), (uint32_t)i, -80});
  size_t n = fp->evaluate(1000, out, 8);
  TEST_ASSERT_EQUAL(1, n);  // ELRS only; FHSS suppressed
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_elrs900_detected);
  RUN_TEST(test_elrs868_label);
  RUN_TEST(test_elrs24_detected);
  RUN_TEST(test_too_few_channels_no_elrs);
  RUN_TEST(test_mixed_sf_not_combined);
  RUN_TEST(test_hits_outside_window_ignored);
  RUN_TEST(test_fhss_detected);
  RUN_TEST(test_static_carrier_not_fhss);
  RUN_TEST(test_elrs_suppresses_fhss_same_band);
  RUN_TEST(test_rssi_is_max_of_window);
  RUN_TEST(test_recent_cad_returns_window);
  RUN_TEST(test_ring_overflow_safe);
  return UNITY_END();
}
