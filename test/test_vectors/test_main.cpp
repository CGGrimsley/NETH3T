#include <unity.h>
#include <cstring>
#include "detect/obs_build.h"
#include "selftest/vectors.h"

static const DetectFlags kAll = {true, true, true};
static Observation o;
static IdCache* ids;
static Vec v;
void setUp() { memset(&o, 0, sizeof o); memset(&v, 0, sizeof v); ids = new IdCache(); }
void tearDown() { delete ids; }

static void check_rid_wifi() {
  TEST_ASSERT_EQUAL((int)SourceType::REMOTEID_WIFI, (int)o.type);
  TEST_ASSERT_EQUAL_STRING(kVecSerialWifi, o.id);
  TEST_ASSERT_TRUE(o.odid.loc_valid && o.odid.op_valid);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, kVecDroneLat, o.odid.lat);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, kVecDroneLon, o.odid.lon);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, kVecPilotLat, o.odid.op_lat);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, kVecPilotLon, o.odid.op_lon);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 120.0f, o.odid.alt_geo_m);
}
static void test_rid_beacon_vector() {
  vec_rid_beacon(v);
  TEST_ASSERT_TRUE(obs_from_wifi(v.data, v.len, -50, 6, 0, kAll, *ids, o));
  check_rid_wifi();
}
static void test_rid_nan_vector() {
  vec_rid_nan(v);
  TEST_ASSERT_TRUE(obs_from_wifi(v.data, v.len, -50, 6, 0, kAll, *ids, o));
  check_rid_wifi();
}
static void test_rid_ble_vectors_merge() {
  vec_rid_ble_basic(v);
  TEST_ASSERT_TRUE(obs_from_ble(kVecBleMac, v.data, v.len, -60, 0, kAll, *ids, o));
  TEST_ASSERT_EQUAL_STRING(kVecSerialBle, o.id);
  vec_rid_ble_location(v);
  TEST_ASSERT_TRUE(obs_from_ble(kVecBleMac, v.data, v.len, -60, 10, kAll, *ids, o));
  TEST_ASSERT_EQUAL_STRING(kVecSerialBle, o.id);
  TEST_ASSERT_TRUE(o.odid.loc_valid);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, kVecBleLat, o.odid.lat);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, kVecBleLon, o.odid.lon);
}
static void test_tello_vector() {
  vec_tello_beacon(v);
  TEST_ASSERT_TRUE(obs_from_wifi(v.data, v.len, -50, 1, 0, kAll, *ids, o));
  TEST_ASSERT_EQUAL((int)SourceType::DRONE_WIFI, (int)o.type);
  TEST_ASSERT_EQUAL_STRING("Ryze/DJI Tello", o.vendor);
  TEST_ASSERT_EQUAL_STRING("TELLO-SELFTEST", o.label);
}
static void test_mesh_vector() {
  vec_mesh_packet(v);
  obs_from_lora_packet(PlanKind::MESHTASTIC, 906875, 11, 250000, v.data, v.len, -90, 5, 0, o);
  TEST_ASSERT_EQUAL((int)SourceType::MESHTASTIC, (int)o.type);
  TEST_ASSERT_EQUAL_STRING("!5e1f7e57", o.id);
}
static void test_lorawan_vector() {
  vec_lorawan_uplink(v);
  obs_from_lora_packet(PlanKind::LORAWAN, 903900, 7, 125000, v.data, v.len, -95, 3, 0, o);
  TEST_ASSERT_EQUAL((int)SourceType::LORAWAN, (int)o.type);
  TEST_ASSERT_EQUAL_STRING("DA:26011BDA", o.id);
}
static void test_elrs_vector() {
  CadHit h[8];
  size_t n = vec_elrs_hits(h, 8, 1000);
  TEST_ASSERT_EQUAL(6, n);
  RcFingerprint fp;
  for (size_t i = 0; i < n; i++) fp.add_cad(h[i]);
  Observation out[4];
  TEST_ASSERT_EQUAL(1, fp.evaluate(1700, out, 4));
  TEST_ASSERT_EQUAL_STRING("ELRS900-SF7", out[0].id);
  TEST_ASSERT_EQUAL(100, out[0].lora.rate_hz);
}
static void test_vectors_fit_buffers() {
  void (*fns[])(Vec&) = {vec_rid_beacon, vec_rid_nan, vec_rid_ble_basic, vec_rid_ble_location,
                         vec_tello_beacon, vec_mesh_packet, vec_lorawan_uplink};
  for (auto fn : fns) {
    fn(v);
    TEST_ASSERT_TRUE(v.len > 0 && v.len <= sizeof v.data);
    TEST_ASSERT_NOT_NULL(v.name);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_rid_beacon_vector);
  RUN_TEST(test_rid_nan_vector);
  RUN_TEST(test_rid_ble_vectors_merge);
  RUN_TEST(test_tello_vector);
  RUN_TEST(test_mesh_vector);
  RUN_TEST(test_lorawan_vector);
  RUN_TEST(test_elrs_vector);
  RUN_TEST(test_vectors_fit_buffers);
  return UNITY_END();
}
