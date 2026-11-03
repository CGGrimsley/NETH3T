#include <unity.h>
#include "ui/spectrum_view.h"
void setUp() {}
void tearDown() {}

static const BandLimits k915 = {902000, 928000, 4000};
static const BandLimits k433 = {433050, 434790, 4000};

static void test_band_limits() {
  BandLimits b = band_limits(0, Region::FCC);
  TEST_ASSERT_EQUAL_UINT32(902000, b.lo_khz);
  TEST_ASSERT_EQUAL_UINT32(928000, b.hi_khz);
  TEST_ASSERT_EQUAL_UINT32(863000, band_limits(0, Region::EU).lo_khz);
  TEST_ASSERT_EQUAL_UINT32(16000, band_limits(2, Region::FCC).min_span_khz);
}
static void test_zoom_in_halves_around_center() {
  Span s = zoom_in(full_span(k915), 915000, k915);
  TEST_ASSERT_EQUAL_UINT32(13000, s.f1_khz - s.f0_khz);
  TEST_ASSERT_EQUAL_UINT32(908500, s.f0_khz);
}
static void test_zoom_in_respects_min_span() {
  Span s = full_span(k915);
  for (int i = 0; i < 10; i++) s = zoom_in(s, 915000, k915);
  TEST_ASSERT_EQUAL_UINT32(4000, s.f1_khz - s.f0_khz);
  TEST_ASSERT_EQUAL_UINT32(913000, s.f0_khz);
}
static void test_zoom_clamps_at_band_edge() {
  Span s = zoom_in(full_span(k915), 902000, k915);
  TEST_ASSERT_EQUAL_UINT32(902000, s.f0_khz);
  TEST_ASSERT_TRUE(s.f1_khz > s.f0_khz);
  TEST_ASSERT_EQUAL_UINT32(915000, s.f1_khz);
  Span t = zoom_in(full_span(k915), 928000, k915);
  TEST_ASSERT_EQUAL_UINT32(928000, t.f1_khz);
}
static void test_zoom_out_to_full_band() {
  Span s = {913000, 917000};
  for (int i = 0; i < 6; i++) s = zoom_out(s, 915000, k915);
  TEST_ASSERT_EQUAL_UINT32(902000, s.f0_khz);
  TEST_ASSERT_EQUAL_UINT32(928000, s.f1_khz);
}
static void test_433_cannot_zoom() {
  Span s = zoom_in(full_span(k433), 434000, k433);
  TEST_ASSERT_EQUAL_UINT32(433050, s.f0_khz);
  TEST_ASSERT_EQUAL_UINT32(434790, s.f1_khz);
}
static void test_freq_x_roundtrip() {
  Span s = {902000, 928000};
  TEST_ASSERT_EQUAL(0, freq_to_x(902000, s, 480));
  TEST_ASSERT_EQUAL(479, freq_to_x(928000, s, 480));
  TEST_ASSERT_EQUAL(0, freq_to_x(800000, s, 480));
  for (int x = 0; x < 480; x += 37) {
    uint32_t f = x_to_freq(x, s, 480);
    TEST_ASSERT_INT_WITHIN(1, x, freq_to_x(f, s, 480));
  }
}
static void test_peak_hold_max_and_decay() {
  PeakHold p;
  int8_t a[2] = {-60, -90};
  int8_t b[2] = {-80, -70};
  p.update(a, 2, 0);
  p.update(b, 2, 5000);
  TEST_ASSERT_EQUAL(-65, p.at(0));
  TEST_ASSERT_EQUAL(-70, p.at(1));
}
static void test_peak_resets_on_range_change() {
  PeakHold p;
  int8_t a[2] = {-40, -40};
  int8_t b[3] = {-90, -91, -92};
  p.update(a, 2, 0);
  p.update(b, 3, 100);  // different bin count: new range
  TEST_ASSERT_EQUAL(3, p.n());
  TEST_ASSERT_EQUAL(-90, p.at(0));
  p.reset();
  int8_t c[3] = {-100, -100, -100};
  p.update(c, 3, 200);
  TEST_ASSERT_EQUAL(-100, p.at(0));
}
static void test_touch_to_landscape_corners() {
  int16_t x, y;
  touch_to_landscape(0, 0, 1, &x, &y);
  TEST_ASSERT_EQUAL(0, x); TEST_ASSERT_EQUAL(221, y);
  touch_to_landscape(221, 479, 1, &x, &y);
  TEST_ASSERT_EQUAL(479, x); TEST_ASSERT_EQUAL(0, y);
  touch_to_landscape(0, 0, 3, &x, &y);
  TEST_ASSERT_EQUAL(479, x); TEST_ASSERT_EQUAL(0, y);
  touch_to_landscape(221, 479, 3, &x, &y);
  TEST_ASSERT_EQUAL(0, x); TEST_ASSERT_EQUAL(221, y);
}

static void test_sweep_matches_requires_same_range() {
  ScanPlan zoom = make_sweep_plan("", 902000, 915000, 50, false);
  ScanPlan full = make_sweep_plan("", 902000, 928000, 50, false);
  TEST_ASSERT_EQUAL(full.n_freqs, zoom.n_freqs);  // same f0 and n: the old match could not tell them apart
  TEST_ASSERT_TRUE(sweep_matches(902000, zoom.freqs_khz[zoom.n_freqs - 1], zoom.n_freqs, zoom));
  TEST_ASSERT_FALSE(sweep_matches(902000, full.freqs_khz[full.n_freqs - 1], full.n_freqs, zoom));
  ScanPlan f24 = make_sweep_plan("", 2400000, 2480000, 200, true);
  TEST_ASSERT_FALSE(sweep_matches(2400000, 2479000, 80, f24));
}
static void test_snap_to_grid() {
  TEST_ASSERT_EQUAL_UINT32(915000, snap_khz(915040, 200));
  TEST_ASSERT_EQUAL_UINT32(915200, snap_khz(915110, 200));
  TEST_ASSERT_EQUAL_UINT32(902000, snap_khz(902000, 200));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_band_limits);
  RUN_TEST(test_zoom_in_halves_around_center);
  RUN_TEST(test_zoom_in_respects_min_span);
  RUN_TEST(test_zoom_clamps_at_band_edge);
  RUN_TEST(test_zoom_out_to_full_band);
  RUN_TEST(test_433_cannot_zoom);
  RUN_TEST(test_freq_x_roundtrip);
  RUN_TEST(test_peak_hold_max_and_decay);
  RUN_TEST(test_peak_resets_on_range_change);
  RUN_TEST(test_touch_to_landscape_corners);
  RUN_TEST(test_sweep_matches_requires_same_range);
  RUN_TEST(test_snap_to_grid);
  return UNITY_END();
}
