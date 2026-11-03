#pragma once
// Golden inputs for the on-device self-test. Pure; verified on the host by test_vectors.
#include <cstddef>
#include <cstdint>
#include "detect/rc_fingerprint.h"

struct Vec { const char* name; size_t len; uint8_t data[200]; };

// Remote ID serials / positions used by the vectors (spec §2.2).
constexpr const char* kVecSerialWifi = "1596F0SELFTEST01";
constexpr const char* kVecSerialBle = "1596F0SELFTEST02";
constexpr double kVecDroneLat = 35.7796, kVecDroneLon = -78.6382;
constexpr double kVecPilotLat = 35.7790, kVecPilotLon = -78.6390;
constexpr double kVecBleLat = 35.7800, kVecBleLon = -78.6400;
extern const uint8_t kVecWifiMac[6];   // 02:00:00:00:00:03
extern const uint8_t kVecTelloMac[6];  // 02:00:00:00:00:01
extern const uint8_t kVecBleMac[6];    // 02:00:00:00:00:02

void vec_rid_beacon(Vec& v);
void vec_rid_nan(Vec& v);
void vec_rid_ble_basic(Vec& v);
void vec_rid_ble_location(Vec& v);
void vec_tello_beacon(Vec& v);
void vec_mesh_packet(Vec& v);      // sender 0x5e1f7e57
void vec_lorawan_uplink(Vec& v);   // DevAddr 0x26011BDA, FCnt 7
size_t vec_elrs_hits(CadHit* out, size_t max, uint32_t t0);  // 6 SF7/BW500 hits on 4 channels
