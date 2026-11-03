#pragma once
// Shared detection types. Pure: no Arduino / ESP-IDF includes.
#include <cstdint>
#include <cstddef>

enum class SourceType : uint8_t {
  REMOTEID_WIFI, REMOTEID_BLE, DJI_LEGACY, DRONE_WIFI,
  ELRS, FHSS, MESHTASTIC, LORAWAN, LORA_UNKNOWN
};
constexpr size_t kSourceTypeCount = 9;

enum class Confidence : uint8_t { HEURISTIC = 0, FINGERPRINT = 1, DECODED = 2 };
enum class Trend : uint8_t { STEADY, WARMER, COLDER };

struct OdidFields {
  bool has_basic, has_loc, has_sys, has_opid, has_self, auth_present;
  uint8_t id_type, ua_type, status;
  char uas_id[21], operator_id[21], self_id[24];
  bool loc_valid;            // false when position unknown (0,0) or out of range
  double lat, lon;
  float alt_geo_m, height_m, speed_h_ms, speed_v_ms, direction_deg;
  bool op_valid;
  double op_lat, op_lon;
  float op_alt_m;
};

struct LoraFields {
  uint8_t sf;
  uint32_t bw_hz;
  uint16_t rate_hz;          // 0 = unknown
};

struct Observation {
  SourceType type;
  Confidence conf;
  char id[24];
  char label[24];
  char vendor[16];
  uint32_t freq_khz;
  int16_t rssi_dbm;
  int8_t snr_db;
  uint32_t t_ms;
  OdidFields odid;
  LoraFields lora;
};
