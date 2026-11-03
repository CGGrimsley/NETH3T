#include "detect/lora_headers.h"
#include <cstdio>

namespace {
uint32_t le32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
uint64_t le64(const uint8_t* p) { return (uint64_t)le32(p) | ((uint64_t)le32(p + 4) << 32); }
}  // namespace

bool parse_meshtastic(const uint8_t* p, size_t len, MeshHeader& out) {
  if (!p || len < 16) return false;
  out.dest = le32(p);
  out.sender = le32(p + 4);
  out.packet_id = le32(p + 8);
  uint8_t flags = p[12];
  out.hop_limit = flags & 0x07;
  out.want_ack = (flags >> 3) & 1;
  out.hop_start = (flags >> 5) & 0x07;
  out.channel_hash = p[13];
  return true;
}

bool parse_lorawan(const uint8_t* p, size_t len, LwHeader& out) {
  if (!p || len < 1) return false;
  uint8_t mhdr = p[0];
  if ((mhdr & 0x03) != 0) return false;  // LoRaWAN R1 major only
  out = LwHeader{};
  out.type = (LwType)(mhdr >> 5);
  switch (out.type) {
    case LwType::JOIN_REQUEST:
      if (len != 23) return false;
      out.join_eui = le64(p + 1);
      out.dev_eui = le64(p + 9);
      return true;
    case LwType::UNCONF_UP:
    case LwType::UNCONF_DOWN:
    case LwType::CONF_UP:
    case LwType::CONF_DOWN:
      if (len < 12) return false;
      out.dev_addr = le32(p + 1);
      out.fcnt = (uint16_t)(p[6] | (p[7] << 8));
      return true;
    default:
      return false;  // join-accept / rejoin / proprietary: nothing readable to show
  }
}

void format_mesh_id(uint32_t sender, char out[24]) { snprintf(out, 24, "!%08x", (unsigned)sender); }

void format_lw_id(const LwHeader& h, char out[24]) {
  if (h.type == LwType::JOIN_REQUEST) snprintf(out, 24, "EUI:%016llX", (unsigned long long)h.dev_eui);
  else snprintf(out, 24, "DA:%08X", (unsigned)h.dev_addr);
}
