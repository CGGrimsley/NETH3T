#include <unity.h>
#include <cstring>
#include "log/csv_format.h"

static Observation o;
static char buf[512];
void setUp() { memset(&o, 0, sizeof o); memset(buf, 0, sizeof buf); }
void tearDown() {}

static void test_header_exact() {
  TEST_ASSERT_EQUAL_STRING(
      "t_ms,iso_time,type,conf,id,freq_khz,rssi,snr,sf,bw_khz,lat,lon,alt_m,op_lat,op_lon,vendor,extra",
      kCsvHeader);
}
static void test_row_remoteid_full() {
  o.type = SourceType::REMOTEID_BLE; o.conf = Confidence::DECODED; o.t_ms = 1234;
  strcpy(o.id, "SN1"); strcpy(o.label, "RID SN1"); o.freq_khz = 2402000; o.rssi_dbm = -70; o.snr_db = 0;
  o.odid.has_loc = true; o.odid.loc_valid = true; o.odid.lat = 37.7749; o.odid.lon = -122.4194; o.odid.alt_geo_m = 120.5f;
  o.odid.has_sys = true; o.odid.op_valid = true; o.odid.op_lat = 37.7; o.odid.op_lon = -122.4;
  size_t n = csv_row(o, "2026-11-03T12:00:00Z", buf, sizeof buf);
  TEST_ASSERT_EQUAL(strlen(buf), n);
  TEST_ASSERT_EQUAL_STRING(
      "1234,2026-11-03T12:00:00Z,REMOTEID_BLE,DECODED,SN1,2402000,-70,0,,,37.7749000,-122.4194000,120.5,37.7000000,-122.4000000,,RID SN1",
      buf);
}
static void test_row_lora_fields() {
  o.type = SourceType::ELRS; o.conf = Confidence::FINGERPRINT; o.t_ms = 5;
  strcpy(o.id, "ELRS900-SF7"); o.freq_khz = 915500; o.rssi_dbm = -88; o.lora = {7, 500000, 100};
  csv_row(o, "", buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("5,,ELRS,FINGERPRINT,ELRS900-SF7,915500,-88,0,7,500.0,,,,,,,", buf);
}
static void test_row_invalid_loc_empty_fields() {
  o.type = SourceType::REMOTEID_WIFI; o.conf = Confidence::DECODED;
  strcpy(o.id, "X");
  o.odid.has_loc = true; o.odid.loc_valid = false; o.odid.lat = 0; o.odid.lon = 0;
  csv_row(o, "", buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("0,,REMOTEID_WIFI,DECODED,X,0,0,0,,,,,,,,,", buf);
}
static void test_row_quotes_comma_in_ssid() {
  o.type = SourceType::DRONE_WIFI; strcpy(o.id, "AA"); strcpy(o.label, "a,b"); strcpy(o.vendor, "Say \"hi\"");
  csv_row(o, "", buf, sizeof buf);
  TEST_ASSERT_NOT_NULL(strstr(buf, ",\"Say \"\"hi\"\"\",\"a,b\""));
}
static void test_row_truncated_buffer_returns_0() {
  o.type = SourceType::DRONE_WIFI; strcpy(o.id, "AA:BB:CC:DD:EE:FF"); strcpy(o.label, "something long");
  char small[20];
  TEST_ASSERT_EQUAL(0, csv_row(o, "", small, sizeof small));
}
static void test_type_names_cover_all_enums() {
  for (size_t i = 0; i < kSourceTypeCount; i++) {
    const char* n = source_type_name((SourceType)i);
    TEST_ASSERT_NOT_NULL(n);
    TEST_ASSERT_NOT_EQUAL(0, strcmp(n, "?"));
  }
  TEST_ASSERT_EQUAL_STRING("LORA_UNKNOWN", source_type_name(SourceType::LORA_UNKNOWN));
  TEST_ASSERT_EQUAL_STRING("HEURISTIC", conf_name(Confidence::HEURISTIC));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_header_exact);
  RUN_TEST(test_row_remoteid_full);
  RUN_TEST(test_row_lora_fields);
  RUN_TEST(test_row_invalid_loc_empty_fields);
  RUN_TEST(test_row_quotes_comma_in_ssid);
  RUN_TEST(test_row_truncated_buffer_returns_0);
  RUN_TEST(test_type_names_cover_all_enums);
  return UNITY_END();
}
