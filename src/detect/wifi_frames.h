#pragma once
// 802.11 management-frame parser for Remote ID (beacon vendor IE, Wi-Fi NAN),
// legacy DJI DroneID beacons, and plain SSIDs. Pure.
#include <cstddef>
#include <cstdint>

enum class WifiKind : uint8_t { NONE, PLAIN_BEACON, ODID_BEACON, ODID_NAN, DJI_LEGACY };

struct WifiParse {
  WifiKind kind;
  uint8_t src[6];
  char ssid[33];
  const uint8_t* odid;  // points inside the input frame (ODID message pack), or nullptr
  size_t odid_len;
};

// `frame` starts at Frame Control and must not include the FCS.
// Returns false for frames that are not beacon / probe response / action.
bool wifi_parse_mgmt(const uint8_t* frame, size_t len, WifiParse& out);

// First 6 bytes of SHA-256("org.opendroneid.remoteid")
extern const uint8_t kNanOdidServiceId[6];
