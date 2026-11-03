#include <unity.h>
#include <cstring>
#include "detect/channel_plans.h"
void setUp() {}
void tearDown() {}

static const ScanPlan* find(const ScanPlan* p, size_t n, const char* name) {
  for (size_t i = 0; i < n; i++)
    if (strcmp(p[i].name, name) == 0) return &p[i];
  return nullptr;
}
static ScanPlan plans[kMaxPlans];

static void test_meshtastic_longfast_us_906875() {
  TEST_ASSERT_EQUAL_UINT32(906875, meshtastic_freq_khz(Region::FCC, "LongFast", 250000));
}
static void test_meshtastic_mediumfast_us_913125() {
  TEST_ASSERT_EQUAL_UINT32(913125, meshtastic_freq_khz(Region::FCC, "MediumFast", 250000));
}
static void test_meshtastic_eu_single_slot_869525() {
  TEST_ASSERT_EQUAL_UINT32(869525, meshtastic_freq_khz(Region::EU, "LongFast", 250000));
}
static void test_elrs900_fcc_40ch() {
  size_t n = build_plans(Region::FCC, plans, kMaxPlans);
  const ScanPlan* p = find(plans, n, "ELRS900");
  TEST_ASSERT_NOT_NULL(p);
  TEST_ASSERT_EQUAL(40, p->n_freqs);
  TEST_ASSERT_EQUAL_UINT32(903500, p->freqs_khz[0]);
  TEST_ASSERT_EQUAL_UINT32(926900, p->freqs_khz[39]);
  TEST_ASSERT_EQUAL_UINT32(500000, p->bw_hz);
  TEST_ASSERT_EQUAL((int)PlanMode::CAD, (int)p->mode);
  TEST_ASSERT_EQUAL(4, p->n_sfs);
  TEST_ASSERT_EQUAL(6, p->sfs[0]);
  TEST_ASSERT_EQUAL(9, p->sfs[3]);
}
static void test_elrs900_eu_13ch() {
  size_t n = build_plans(Region::EU, plans, kMaxPlans);
  const ScanPlan* p = find(plans, n, "ELRS900");
  TEST_ASSERT_EQUAL(13, p->n_freqs);
  TEST_ASSERT_EQUAL_UINT32(863275, p->freqs_khz[0]);
  TEST_ASSERT_EQUAL_UINT32(869575, p->freqs_khz[12]);
}
static void test_elrs24_80ch() {
  size_t n = build_plans(Region::FCC, plans, kMaxPlans);
  const ScanPlan* p = find(plans, n, "ELRS24");
  TEST_ASSERT_EQUAL(80, p->n_freqs);
  TEST_ASSERT_EQUAL_UINT32(2400400, p->freqs_khz[0]);
  TEST_ASSERT_EQUAL_UINT32(2479400, p->freqs_khz[79]);
  TEST_ASSERT_EQUAL_UINT32(812500, p->bw_hz);
  TEST_ASSERT_TRUE(p->hf);
  TEST_ASSERT_EQUAL(5, p->sfs[0]);
}
static void test_mesh_and_lorawan_plans() {
  size_t n = build_plans(Region::FCC, plans, kMaxPlans);
  const ScanPlan* m = find(plans, n, "MESH LongFast");
  TEST_ASSERT_NOT_NULL(m);
  TEST_ASSERT_EQUAL_HEX8(0x2B, m->sync_word);
  TEST_ASSERT_EQUAL(11, m->sfs[0]);
  TEST_ASSERT_EQUAL_UINT32(906875, m->freqs_khz[0]);
  TEST_ASSERT_EQUAL(1500, m->rx_timeout_ms);
  const ScanPlan* l = find(plans, n, "LORAWAN");
  TEST_ASSERT_EQUAL_HEX8(0x34, l->sync_word);
  TEST_ASSERT_EQUAL(8, l->n_freqs);
  TEST_ASSERT_EQUAL_UINT32(903900, l->freqs_khz[0]);
  TEST_ASSERT_EQUAL_UINT32(905300, l->freqs_khz[7]);
  size_t ne = build_plans(Region::EU, plans, kMaxPlans);
  const ScanPlan* le = find(plans, ne, "LORAWAN");
  TEST_ASSERT_EQUAL(3, le->n_freqs);
  TEST_ASSERT_EQUAL_UINT32(868100, le->freqs_khz[0]);
}
static void test_elrs_rate_map() {
  TEST_ASSERT_EQUAL(200, elrs_rate_hz(false, 6));
  TEST_ASSERT_EQUAL(100, elrs_rate_hz(false, 7));
  TEST_ASSERT_EQUAL(50, elrs_rate_hz(false, 8));
  TEST_ASSERT_EQUAL(25, elrs_rate_hz(false, 9));
  TEST_ASSERT_EQUAL(500, elrs_rate_hz(true, 5));
  TEST_ASSERT_EQUAL(250, elrs_rate_hz(true, 6));
  TEST_ASSERT_EQUAL(150, elrs_rate_hz(true, 7));
  TEST_ASSERT_EQUAL(50, elrs_rate_hz(true, 8));
  TEST_ASSERT_EQUAL(0, elrs_rate_hz(false, 12));
}
static void test_region_switch_rebuilds_plans() {
  size_t n = build_plans(Region::EU, plans, kMaxPlans);
  for (size_t i = 0; i < n; i++)
    for (size_t k = 0; k < plans[i].n_freqs; k++)
      TEST_ASSERT_FALSE(plans[i].freqs_khz[k] >= 902000 && plans[i].freqs_khz[k] <= 928000);
  n = build_plans(Region::FCC, plans, kMaxPlans);
  for (size_t i = 0; i < n; i++)
    for (size_t k = 0; k < plans[i].n_freqs; k++)
      TEST_ASSERT_FALSE(plans[i].freqs_khz[k] >= 863000 && plans[i].freqs_khz[k] <= 870000);
}
static void test_sweep_respects_80_point_cap() {
  size_t n = build_plans(Region::FCC, plans, kMaxPlans);
  const ScanPlan* s = find(plans, n, "SWEEP 915");
  TEST_ASSERT_NOT_NULL(s);
  TEST_ASSERT_EQUAL((int)PlanMode::RSSI_SWEEP, (int)s->mode);
  TEST_ASSERT_EQUAL(80, s->n_freqs);
  TEST_ASSERT_EQUAL_UINT32(902000, s->freqs_khz[0]);
  TEST_ASSERT_TRUE(s->freqs_khz[79] <= 928000 && s->freqs_khz[79] > 927000);
  const ScanPlan* e = find(plans, n, "SWEEP 2400");
  TEST_ASSERT_TRUE(e->hf);
  TEST_ASSERT_TRUE(e->n_freqs <= 80);
  size_t ne = build_plans(Region::EU, plans, kMaxPlans);
  const ScanPlan* s8 = find(plans, ne, "SWEEP 868");
  TEST_ASSERT_EQUAL(36, s8->n_freqs);
  TEST_ASSERT_EQUAL_UINT32(863000, s8->freqs_khz[0]);
  TEST_ASSERT_EQUAL_UINT32(870000, s8->freqs_khz[35]);
  TEST_ASSERT_NOT_NULL(find(plans, ne, "SWEEP 433"));
}
static void test_build_plans_respects_max() {
  TEST_ASSERT_EQUAL(3, build_plans(Region::FCC, plans, 3));
}

static void test_make_sweep_plan_points() {
  ScanPlan p = make_sweep_plan("Z", 913000, 917000, 50, false);
  TEST_ASSERT_EQUAL(80, p.n_freqs);
  TEST_ASSERT_EQUAL_UINT32(913000, p.freqs_khz[0]);
  TEST_ASSERT_EQUAL_UINT32(917000, p.freqs_khz[79]);
  TEST_ASSERT_EQUAL((int)PlanMode::RSSI_SWEEP, (int)p.mode);
  ScanPlan q = make_sweep_plan("Z", 915000, 915500, 50, false);  // narrower than 80 steps
  TEST_ASSERT_EQUAL(11, q.n_freqs);
  TEST_ASSERT_EQUAL_UINT32(915500, q.freqs_khz[10]);
  ScanPlan h = make_sweep_plan("H", 2400000, 2416000, 200, true);
  TEST_ASSERT_TRUE(h.hf);
  TEST_ASSERT_EQUAL(80, h.n_freqs);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_meshtastic_longfast_us_906875);
  RUN_TEST(test_meshtastic_mediumfast_us_913125);
  RUN_TEST(test_meshtastic_eu_single_slot_869525);
  RUN_TEST(test_elrs900_fcc_40ch);
  RUN_TEST(test_elrs900_eu_13ch);
  RUN_TEST(test_elrs24_80ch);
  RUN_TEST(test_mesh_and_lorawan_plans);
  RUN_TEST(test_elrs_rate_map);
  RUN_TEST(test_region_switch_rebuilds_plans);
  RUN_TEST(test_sweep_respects_80_point_cap);
  RUN_TEST(test_build_plans_respects_max);
  RUN_TEST(test_make_sweep_plan_points);
  return UNITY_END();
}
