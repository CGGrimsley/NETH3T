#include <unity.h>
#include <cstring>
#include <cstdio>
#include "core/target_db.h"

static TargetDb* db;
void setUp() { db = new TargetDb(); }
void tearDown() { delete db; }

static Observation obs(SourceType type, const char* id, int16_t rssi, uint32_t t,
                       Confidence c = Confidence::HEURISTIC) {
  Observation o;
  memset(&o, 0, sizeof(o));
  o.type = type; o.conf = c; o.rssi_dbm = rssi; o.t_ms = t;
  strncpy(o.id, id, sizeof(o.id) - 1);
  return o;
}

static void test_merge_same_type_and_id() {
  db->ingest(obs(SourceType::DRONE_WIFI, "A", -70, 0));
  db->ingest(obs(SourceType::DRONE_WIFI, "A", -60, 100));
  TEST_ASSERT_EQUAL(1, db->size());
  TEST_ASSERT_EQUAL(2, db->find(SourceType::DRONE_WIFI, "A")->hits);
}

static void test_same_id_different_type_separate() {
  db->ingest(obs(SourceType::DRONE_WIFI, "A", -70, 0));
  db->ingest(obs(SourceType::MESHTASTIC, "A", -70, 0));
  TEST_ASSERT_EQUAL(2, db->size());
}

static void test_ema_smoothing() {
  db->ingest(obs(SourceType::ELRS, "E", -80, 0));
  db->ingest(obs(SourceType::ELRS, "E", -60, 100));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -74.0f, db->find(SourceType::ELRS, "E")->ema_rssi);
}

static void test_peak_hold_and_reset() {
  db->ingest(obs(SourceType::ELRS, "E", -80, 0));
  db->ingest(obs(SourceType::ELRS, "E", -50, 100));
  db->ingest(obs(SourceType::ELRS, "E", -70, 200));
  Target* t = db->find(SourceType::ELRS, "E");
  TEST_ASSERT_EQUAL(-50, t->peak_rssi);
  db->reset_peak(SourceType::ELRS, "E");
  TEST_ASSERT_EQUAL(-70, t->peak_rssi);
}

static void feed(const char* id, const int16_t* r, const uint32_t* t, int n) {
  for (int i = 0; i < n; i++) db->ingest(obs(SourceType::ELRS, id, r[i], t[i]));
}

static void test_trend_warmer_colder_steady() {
  // Rising: many samples so EMA climbs well beyond 2 dB in 3 s
  int16_t up[8] = {-90, -85, -80, -75, -70, -65, -60, -60};
  uint32_t tt[8] = {0, 500, 1000, 1500, 2000, 2500, 3000, 3500};
  feed("U", up, tt, 8);
  TEST_ASSERT_EQUAL((int)Trend::WARMER, (int)trend_of(*db->find(SourceType::ELRS, "U"), 3500));
  int16_t dn[8] = {-60, -65, -70, -75, -80, -85, -90, -90};
  feed("D", dn, tt, 8);
  TEST_ASSERT_EQUAL((int)Trend::COLDER, (int)trend_of(*db->find(SourceType::ELRS, "D"), 3500));
  // Flat within ±1.5 dB
  int16_t fl[8] = {-70, -71, -70, -71, -70, -71, -70, -71};
  feed("F", fl, tt, 8);
  TEST_ASSERT_EQUAL((int)Trend::STEADY, (int)trend_of(*db->find(SourceType::ELRS, "F"), 3500));
}

static void test_trend_single_sample_steady() {
  db->ingest(obs(SourceType::ELRS, "S", -50, 1000));
  TEST_ASSERT_EQUAL((int)Trend::STEADY, (int)trend_of(*db->find(SourceType::ELRS, "S"), 1000));
}

static void test_trend_after_gap() {
  db->ingest(obs(SourceType::ELRS, "G", -90, 0));
  db->ingest(obs(SourceType::ELRS, "G", -60, 10000));
  TEST_ASSERT_EQUAL((int)Trend::WARMER, (int)trend_of(*db->find(SourceType::ELRS, "G"), 10000));
}

static void test_stale_then_expire() {
  db->ingest(obs(SourceType::DRONE_WIFI, "A", -70, 0));
  db->tick(21000);
  TEST_ASSERT_TRUE(db->find(SourceType::DRONE_WIFI, "A")->stale);
  db->tick(61000);
  TEST_ASSERT_NULL(db->find(SourceType::DRONE_WIFI, "A"));
  TEST_ASSERT_EQUAL(0, db->size());
}

static void test_stale_clears_on_new_hit() {
  db->ingest(obs(SourceType::DRONE_WIFI, "A", -70, 0));
  db->tick(21000);
  db->ingest(obs(SourceType::DRONE_WIFI, "A", -70, 22000));
  TEST_ASSERT_FALSE(db->find(SourceType::DRONE_WIFI, "A")->stale);
}

static void test_locked_never_expires() {
  db->ingest(obs(SourceType::DRONE_WIFI, "A", -70, 0));
  TEST_ASSERT_TRUE(db->lock(SourceType::DRONE_WIFI, "A"));
  db->tick(120000);
  TEST_ASSERT_NOT_NULL(db->find(SourceType::DRONE_WIFI, "A"));
  TEST_ASSERT_NOT_NULL(db->locked());
  db->unlock();
  db->tick(120001);
  TEST_ASSERT_NULL(db->find(SourceType::DRONE_WIFI, "A"));
}

static void test_eviction_prefers_low_conf_old_and_never_locked() {
  char id[24];
  // slot 0: locked heuristic, oldest of all
  db->ingest(obs(SourceType::DRONE_WIFI, "LOCK", -70, 0));
  db->lock(SourceType::DRONE_WIFI, "LOCK");
  for (int i = 0; i < 10; i++) {
    snprintf(id, sizeof id, "D%d", i);
    db->ingest(obs(SourceType::REMOTEID_BLE, id, -70, 1, Confidence::DECODED));
  }
  for (int i = 0; i < 117; i++) {
    snprintf(id, sizeof id, "H%d", i);
    db->ingest(obs(SourceType::DRONE_WIFI, id, -70, 100 + i));
  }
  TEST_ASSERT_EQUAL(128, db->size());
  db->ingest(obs(SourceType::ELRS, "NEW", -70, 5000, Confidence::FINGERPRINT));
  TEST_ASSERT_EQUAL(128, db->size());
  TEST_ASSERT_NULL(db->find(SourceType::DRONE_WIFI, "H0"));
  TEST_ASSERT_NOT_NULL(db->find(SourceType::DRONE_WIFI, "H1"));
  TEST_ASSERT_NOT_NULL(db->find(SourceType::DRONE_WIFI, "LOCK"));
  TEST_ASSERT_NOT_NULL(db->find(SourceType::ELRS, "NEW"));
  for (int i = 0; i < 10; i++) {
    snprintf(id, sizeof id, "D%d", i);
    TEST_ASSERT_NOT_NULL(db->find(SourceType::REMOTEID_BLE, id));
  }
}

static void test_snapshot_order() {
  db->ingest(obs(SourceType::DRONE_WIFI, "H", -40, 0));
  db->ingest(obs(SourceType::REMOTEID_BLE, "D", -90, 0, Confidence::DECODED));
  db->ingest(obs(SourceType::DRONE_WIFI, "H2", -30, 0));
  Target out[8];
  size_t n = db->snapshot(out, 8);
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL_STRING("D", out[0].id);
  TEST_ASSERT_EQUAL_STRING("H2", out[1].id);
  TEST_ASSERT_EQUAL_STRING("H", out[2].id);
}

static void test_odid_fields_merge() {
  Observation a = obs(SourceType::REMOTEID_BLE, "SN1", -70, 0, Confidence::DECODED);
  a.odid.has_basic = true; strcpy(a.odid.uas_id, "SN1"); a.odid.ua_type = 2;
  Observation b = obs(SourceType::REMOTEID_BLE, "SN1", -70, 10, Confidence::DECODED);
  b.odid.has_loc = true; b.odid.loc_valid = true; b.odid.lat = 1.5; b.odid.lon = 2.5;
  db->ingest(a); db->ingest(b);
  Target* t = db->find(SourceType::REMOTEID_BLE, "SN1");
  TEST_ASSERT_TRUE(t->odid.has_basic && t->odid.has_loc);
  TEST_ASSERT_EQUAL(2, t->odid.ua_type);
  TEST_ASSERT_EQUAL_DOUBLE(1.5, t->odid.lat);
}

static void test_conf_upgrades_never_downgrades() {
  db->ingest(obs(SourceType::DJI_LEGACY, "X", -70, 0, Confidence::HEURISTIC));
  db->ingest(obs(SourceType::DJI_LEGACY, "X", -70, 1, Confidence::DECODED));
  db->ingest(obs(SourceType::DJI_LEGACY, "X", -70, 2, Confidence::HEURISTIC));
  TEST_ASSERT_EQUAL((int)Confidence::DECODED, (int)db->find(SourceType::DJI_LEGACY, "X")->conf);
}

static void test_remoteid_wifi_and_ble_merge_by_serial() {
  db->ingest(obs(SourceType::REMOTEID_BLE, "SN9", -70, 0, Confidence::DECODED));
  db->ingest(obs(SourceType::REMOTEID_WIFI, "SN9", -65, 10, Confidence::DECODED));
  TEST_ASSERT_EQUAL(1, db->size());
  Target out[2];
  db->snapshot(out, 2);
  TEST_ASSERT_EQUAL((int)SourceType::REMOTEID_WIFI, (int)out[0].type);
  TEST_ASSERT_NOT_NULL(db->find(SourceType::REMOTEID_BLE, "SN9"));
}

static void test_full_table_keeps_drone_over_lora_nodes() {
  char id[24];
  // One older Remote ID drone, then 127 newer DECODED LoRaWAN nodes fill the table.
  db->ingest(obs(SourceType::REMOTEID_BLE, "DRONE", -80, 0, Confidence::DECODED));
  for (int i = 0; i < 127; i++) {
    snprintf(id, sizeof id, "DA:%08X", i);
    db->ingest(obs(SourceType::LORAWAN, id, -90, 100 + i, Confidence::DECODED));
  }
  db->ingest(obs(SourceType::LORAWAN, "DA:NEW", -90, 5000, Confidence::DECODED));
  TEST_ASSERT_NOT_NULL(db->find(SourceType::REMOTEID_BLE, "DRONE"));
  TEST_ASSERT_NOT_NULL(db->find(SourceType::LORAWAN, "DA:NEW"));
  TEST_ASSERT_NULL(db->find(SourceType::LORAWAN, "DA:00000000"));
}

static void test_full_table_of_drones_drops_heuristic_newcomer() {
  char id[24];
  for (int i = 0; i < 128; i++) {
    snprintf(id, sizeof id, "SN%d", i);
    db->ingest(obs(SourceType::REMOTEID_BLE, id, -80, (uint32_t)i, Confidence::DECODED));
  }
  db->ingest(obs(SourceType::DRONE_WIFI, "HOTSPOT", -60, 5000));
  TEST_ASSERT_NULL(db->find(SourceType::DRONE_WIFI, "HOTSPOT"));
  TEST_ASSERT_NOT_NULL(db->find(SourceType::REMOTEID_BLE, "SN0"));
}

static void test_basic_id_serial_not_overwritten_by_caa_id() {
  Observation a = obs(SourceType::REMOTEID_BLE, "SN1", -70, 0, Confidence::DECODED);
  a.odid.has_basic = true; a.odid.id_type = 1; strcpy(a.odid.uas_id, "SERIAL123");
  Observation b = obs(SourceType::REMOTEID_BLE, "SN1", -70, 10, Confidence::DECODED);
  b.odid.has_basic = true; b.odid.id_type = 2; strcpy(b.odid.uas_id, "CAA-REG-9");
  db->ingest(a); db->ingest(b);
  TEST_ASSERT_EQUAL_STRING("SERIAL123", db->find(SourceType::REMOTEID_BLE, "SN1")->odid.uas_id);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_merge_same_type_and_id);
  RUN_TEST(test_same_id_different_type_separate);
  RUN_TEST(test_ema_smoothing);
  RUN_TEST(test_peak_hold_and_reset);
  RUN_TEST(test_trend_warmer_colder_steady);
  RUN_TEST(test_trend_single_sample_steady);
  RUN_TEST(test_trend_after_gap);
  RUN_TEST(test_stale_then_expire);
  RUN_TEST(test_stale_clears_on_new_hit);
  RUN_TEST(test_locked_never_expires);
  RUN_TEST(test_eviction_prefers_low_conf_old_and_never_locked);
  RUN_TEST(test_snapshot_order);
  RUN_TEST(test_odid_fields_merge);
  RUN_TEST(test_conf_upgrades_never_downgrades);
  RUN_TEST(test_remoteid_wifi_and_ble_merge_by_serial);
  RUN_TEST(test_full_table_keeps_drone_over_lora_nodes);
  RUN_TEST(test_full_table_of_drones_drops_heuristic_newcomer);
  RUN_TEST(test_basic_id_serial_not_overwritten_by_caa_id);
  return UNITY_END();
}
