#include <unity.h>
#include <cstring>
#include <vector>
#include "detect/obs_build.h"
#include "helpers/odid_encode.h"

using Bytes = std::vector<uint8_t>;
static const DetectFlags kAll = {true, true, true};
static const uint8_t kSrc[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x09};
static Observation o;
static IdCache* ids;
void setUp() { memset(&o, 0, sizeof o); ids = new IdCache(); }
void tearDown() { delete ids; }

static Bytes beacon(const uint8_t* src) {
  Bytes b = {0x80, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  b.insert(b.end(), src, src + 6);
  b.insert(b.end(), src, src + 6);
  b.push_back(0x10); b.push_back(0);
  for (int i = 0; i < 12; i++) b.push_back(0);
  return b;
}
static void ssid(Bytes& b, const char* s) { b.push_back(0); b.push_back((uint8_t)strlen(s)); b.insert(b.end(), s, s + strlen(s)); }

static void test_wifi_odid_beacon_to_remoteid() {
  uint8_t msgs[50], pack[53];
  enc_basic(msgs, 1, 2, "SN-OBS-1");
  enc_location(msgs + 25, 10, 20, 100, 30, 5, 90);
  size_t n = enc_pack(pack, msgs, 2);
  Bytes b = beacon(kSrc);
  b.push_back(0xDD); b.push_back((uint8_t)(5 + n));
  uint8_t hdr[5] = {0xFA, 0x0B, 0xBC, 0x0D, 0x01};
  b.insert(b.end(), hdr, hdr + 5);
  b.insert(b.end(), pack, pack + n);
  TEST_ASSERT_TRUE(obs_from_wifi(b.data(), b.size(), -55, 6, 1000, kAll, *ids, o));
  TEST_ASSERT_EQUAL((int)SourceType::REMOTEID_WIFI, (int)o.type);
  TEST_ASSERT_EQUAL((int)Confidence::DECODED, (int)o.conf);
  TEST_ASSERT_EQUAL_STRING("SN-OBS-1", o.id);
  TEST_ASSERT_EQUAL(-55, o.rssi_dbm);
  TEST_ASSERT_EQUAL_UINT32(2437000, o.freq_khz);
  TEST_ASSERT_EQUAL_UINT32(1000, o.t_ms);
  TEST_ASSERT_TRUE(o.odid.loc_valid);
}
static void test_wifi_flags_disable() {
  Bytes b = beacon(kSrc);
  ssid(b, "TELLO-1");
  DetectFlags f = {true, true, false};
  TEST_ASSERT_FALSE(obs_from_wifi(b.data(), b.size(), -55, 1, 0, f, *ids, o));
}
static void test_wifi_tello_ssid_to_drone_wifi() {
  Bytes b = beacon(kSrc);
  ssid(b, "TELLO-SELFTEST");
  TEST_ASSERT_TRUE(obs_from_wifi(b.data(), b.size(), -60, 11, 5, kAll, *ids, o));
  TEST_ASSERT_EQUAL((int)SourceType::DRONE_WIFI, (int)o.type);
  TEST_ASSERT_EQUAL_STRING("Ryze/DJI Tello", o.vendor);
  TEST_ASSERT_EQUAL_STRING("TELLO-SELFTEST", o.label);
  TEST_ASSERT_EQUAL_STRING("02:00:00:00:00:09", o.id);
}
static void test_wifi_plain_router_ignored() {
  Bytes b = beacon(kSrc);
  ssid(b, "HomeWiFi");
  TEST_ASSERT_FALSE(obs_from_wifi(b.data(), b.size(), -60, 1, 0, kAll, *ids, o));
}
static void test_wifi_dji_legacy() {
  Bytes b = beacon(kSrc);
  ssid(b, "x");
  uint8_t ie[8] = {0xDD, 0x06, 0x26, 0x37, 0x12, 0x58, 0x62, 0x13};
  b.insert(b.end(), ie, ie + 8);
  TEST_ASSERT_TRUE(obs_from_wifi(b.data(), b.size(), -70, 1, 0, kAll, *ids, o));
  TEST_ASSERT_EQUAL((int)SourceType::DJI_LEGACY, (int)o.type);
  TEST_ASSERT_EQUAL_STRING("DJI", o.vendor);
}
static Bytes ble_adv(const uint8_t* msg) {
  Bytes b = {0x02, 0x01, 0x06, 0x1E, 0x16, 0xFA, 0xFF, 0x0D, 0x00};
  b.insert(b.end(), msg, msg + 25);
  return b;
}
static void test_ble_basic_then_location_same_id() {
  uint8_t m[25];
  enc_basic(m, 1, 2, "SN-BLE-9");
  Bytes a = ble_adv(m);
  TEST_ASSERT_TRUE(obs_from_ble(kSrc, a.data(), a.size(), -65, 10, kAll, *ids, o));
  TEST_ASSERT_EQUAL_STRING("SN-BLE-9", o.id);
  TEST_ASSERT_EQUAL_UINT32(2402000, o.freq_khz);
  enc_location(m, 35.78, -78.64, 50, 20, 3, 10);
  Bytes l = ble_adv(m);
  TEST_ASSERT_TRUE(obs_from_ble(kSrc, l.data(), l.size(), -64, 20, kAll, *ids, o));
  TEST_ASSERT_EQUAL_STRING("SN-BLE-9", o.id);
  TEST_ASSERT_EQUAL((int)SourceType::REMOTEID_BLE, (int)o.type);
  TEST_ASSERT_TRUE(o.odid.has_loc);
}
static void test_ble_disabled_or_not_rid() {
  uint8_t m[25];
  enc_basic(m, 1, 2, "SN");
  Bytes a = ble_adv(m);
  DetectFlags f = {true, false, true};
  TEST_ASSERT_FALSE(obs_from_ble(kSrc, a.data(), a.size(), -65, 0, f, *ids, o));
  uint8_t plain[3] = {0x02, 0x01, 0x06};
  TEST_ASSERT_FALSE(obs_from_ble(kSrc, plain, 3, -65, 0, kAll, *ids, o));
}
static void test_lora_mesh_packet() {
  uint8_t p[24] = {0xFF, 0xFF, 0xFF, 0xFF, 0x57, 0x7E, 0x1F, 0x5E, 1, 2, 3, 4, 0x63, 0x08, 0, 0};
  obs_from_lora_packet(PlanKind::MESHTASTIC, 906875, 11, 250000, p, sizeof p, -90.5f, 7.2f, 33, o);
  TEST_ASSERT_EQUAL((int)SourceType::MESHTASTIC, (int)o.type);
  TEST_ASSERT_EQUAL_STRING("!5e1f7e57", o.id);
  TEST_ASSERT_EQUAL_STRING("Mesh !5e1f7e57", o.label);
  TEST_ASSERT_EQUAL(-90, o.rssi_dbm);
  TEST_ASSERT_EQUAL(7, o.snr_db);
  TEST_ASSERT_EQUAL(11, o.lora.sf);
  TEST_ASSERT_EQUAL_UINT32(250000, o.lora.bw_hz);
}
static void test_lora_lorawan_packet() {
  uint8_t p[16] = {0x40, 0xDA, 0x1B, 0x01, 0x26, 0x80, 0x07, 0x00, 0x01, 0xAA, 0xBB, 0xCC, 0xDD, 0, 0, 0};
  obs_from_lora_packet(PlanKind::LORAWAN, 904100, 7, 125000, p, sizeof p, -100, 0, 0, o);
  TEST_ASSERT_EQUAL((int)SourceType::LORAWAN, (int)o.type);
  TEST_ASSERT_EQUAL_STRING("DA:26011BDA", o.id);
}
static void test_lora_garbage_unknown() {
  uint8_t p[5] = {1, 2, 3, 4, 5};
  obs_from_lora_packet(PlanKind::LORAWAN, 915200, 7, 125000, p, sizeof p, -100, 0, 0, o);
  TEST_ASSERT_EQUAL((int)SourceType::LORA_UNKNOWN, (int)o.type);
  TEST_ASSERT_EQUAL((int)Confidence::HEURISTIC, (int)o.conf);
  TEST_ASSERT_EQUAL_STRING("LORA-915-SF7", o.id);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_wifi_odid_beacon_to_remoteid);
  RUN_TEST(test_wifi_flags_disable);
  RUN_TEST(test_wifi_tello_ssid_to_drone_wifi);
  RUN_TEST(test_wifi_plain_router_ignored);
  RUN_TEST(test_wifi_dji_legacy);
  RUN_TEST(test_ble_basic_then_location_same_id);
  RUN_TEST(test_ble_disabled_or_not_rid);
  RUN_TEST(test_lora_mesh_packet);
  RUN_TEST(test_lora_lorawan_packet);
  RUN_TEST(test_lora_garbage_unknown);
  return UNITY_END();
}
