#pragma once
// Unencrypted header parsing for Meshtastic and LoRaWAN frames. Pure.
#include <cstddef>
#include <cstdint>

struct MeshHeader {
  uint32_t dest, sender, packet_id;
  uint8_t hop_limit, hop_start, channel_hash;
  bool want_ack;
};
bool parse_meshtastic(const uint8_t* p, size_t len, MeshHeader& out);  // needs len >= 16

enum class LwType : uint8_t { JOIN_REQUEST, JOIN_ACCEPT, UNCONF_UP, UNCONF_DOWN, CONF_UP, CONF_DOWN, REJOIN, PROPRIETARY };
struct LwHeader {
  LwType type;
  uint32_t dev_addr;
  uint16_t fcnt;
  uint64_t join_eui, dev_eui;
};
// Accepts data frames (min 12 bytes) and join-requests (exactly 23 bytes), LoRaWAN major 0.
bool parse_lorawan(const uint8_t* p, size_t len, LwHeader& out);

void format_mesh_id(uint32_t sender, char out[24]);  // "!%08x"
void format_lw_id(const LwHeader& h, char out[24]);  // "DA:%08X" or "EUI:%016llX"
