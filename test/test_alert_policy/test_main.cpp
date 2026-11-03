#include <unity.h>
#include <cstring>
#include "alert/alert_policy.h"

static Target t;
void setUp() { memset(&t, 0, sizeof t); }
void tearDown() {}

static Target& mk(SourceType type, Confidence c) { t.type = type; t.conf = c; return t; }

static void test_mode_off_none() {
  TEST_ASSERT_EQUAL((int)AlertLevel::NONE, (int)alert_for(mk(SourceType::REMOTEID_BLE, Confidence::DECODED), 0));
}
static void test_already_alerted_none() {
  mk(SourceType::REMOTEID_BLE, Confidence::DECODED).alerted = true;
  TEST_ASSERT_EQUAL((int)AlertLevel::NONE, (int)alert_for(t, 2));
}
static void test_decoded_drone_alarm() {
  TEST_ASSERT_EQUAL((int)AlertLevel::ALARM, (int)alert_for(mk(SourceType::REMOTEID_BLE, Confidence::DECODED), 2));
  TEST_ASSERT_EQUAL((int)AlertLevel::ALARM, (int)alert_for(mk(SourceType::REMOTEID_WIFI, Confidence::DECODED), 1));
}
static void test_elrs_alarm() {
  TEST_ASSERT_EQUAL((int)AlertLevel::ALARM, (int)alert_for(mk(SourceType::ELRS, Confidence::FINGERPRINT), 2));
}
static void test_heuristic_notice() {
  TEST_ASSERT_EQUAL((int)AlertLevel::NOTICE, (int)alert_for(mk(SourceType::DRONE_WIFI, Confidence::HEURISTIC), 2));
  TEST_ASSERT_EQUAL((int)AlertLevel::NOTICE, (int)alert_for(mk(SourceType::FHSS, Confidence::HEURISTIC), 2));
}
static void test_dji_legacy_notice() {
  TEST_ASSERT_EQUAL((int)AlertLevel::NOTICE, (int)alert_for(mk(SourceType::DJI_LEGACY, Confidence::FINGERPRINT), 2));
}
static void test_lora_nodes_never_alert() {
  TEST_ASSERT_EQUAL((int)AlertLevel::NONE, (int)alert_for(mk(SourceType::MESHTASTIC, Confidence::DECODED), 2));
  TEST_ASSERT_EQUAL((int)AlertLevel::NONE, (int)alert_for(mk(SourceType::LORAWAN, Confidence::DECODED), 2));
  TEST_ASSERT_EQUAL((int)AlertLevel::NONE, (int)alert_for(mk(SourceType::LORA_UNKNOWN, Confidence::HEURISTIC), 2));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_mode_off_none);
  RUN_TEST(test_already_alerted_none);
  RUN_TEST(test_decoded_drone_alarm);
  RUN_TEST(test_elrs_alarm);
  RUN_TEST(test_heuristic_notice);
  RUN_TEST(test_dji_legacy_notice);
  RUN_TEST(test_lora_nodes_never_alert);
  return UNITY_END();
}
