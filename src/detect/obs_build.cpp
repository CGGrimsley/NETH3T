#include "detect/obs_build.h"
#include <cstdio>
#include <cstring>
#include "detect/ble_adv.h"
#include "detect/drone_vendors.h"
#include "detect/lora_headers.h"
#include "detect/odid.h"
#include "detect/wifi_frames.h"

namespace {
// Decodes ODID and fills type/conf/id/label. False if nothing decoded.
bool rid(SourceType type, const uint8_t mac[6], const uint8_t* odid, size_t odid_len, IdCache& ids,
         uint32_t now, Observation& o) {
  memset(&o, 0, sizeof o);
  if (odid_decode(odid, odid_len, o.odid) != OdidErr::OK) return false;
  if (!(o.odid.has_basic || o.odid.has_loc || o.odid.has_sys || o.odid.has_opid || o.odid.has_self))
    return false;
  if (o.odid.has_basic) ids.remember(mac, o.odid.uas_id, now, o.odid.id_type == 1);
  ids.resolve(mac, o.id);
  o.type = type;
  o.conf = Confidence::DECODED;
  snprintf(o.label, sizeof o.label, "RID %s", o.id);
  return true;
}
}  // namespace

bool obs_from_wifi(const uint8_t* frame, size_t len, int8_t rssi, uint8_t ch, uint32_t now,
                   const DetectFlags& f, IdCache& ids, Observation& o) {
  WifiParse w;
  if (!wifi_parse_mgmt(frame, len, w)) return false;
  switch (w.kind) {
    case WifiKind::ODID_BEACON:
    case WifiKind::ODID_NAN:
      if (!f.rid_wifi) return false;
      if (!rid(SourceType::REMOTEID_WIFI, w.src, w.odid, w.odid_len, ids, now, o)) return false;
      break;
    case WifiKind::DJI_LEGACY:
      if (!f.rid_wifi) return false;
      memset(&o, 0, sizeof o);
      o.type = SourceType::DJI_LEGACY;
      o.conf = Confidence::FINGERPRINT;
      format_mac(w.src, o.id);
      snprintf(o.label, sizeof o.label, "DJI DroneID");
      snprintf(o.vendor, sizeof o.vendor, "DJI");
      break;
    case WifiKind::PLAIN_BEACON: {
      if (!f.drone_wifi) return false;
      const char* v = drone_vendor_by_oui(w.src);
      if (!v) v = drone_vendor_by_ssid(w.ssid);
      if (!v) return false;
      memset(&o, 0, sizeof o);
      o.type = SourceType::DRONE_WIFI;
      o.conf = Confidence::HEURISTIC;
      format_mac(w.src, o.id);
      snprintf(o.label, sizeof o.label, "%s", w.ssid[0] ? w.ssid : o.id);
      snprintf(o.vendor, sizeof o.vendor, "%s", v);
      break;
    }
    default:
      return false;
  }
  o.t_ms = now;
  o.rssi_dbm = rssi;
  o.freq_khz = 2407000 + (uint32_t)ch * 5000;
  return true;
}

bool obs_from_ble(const uint8_t mac[6], const uint8_t* adv, size_t len, int8_t rssi, uint32_t now,
                  const DetectFlags& f, IdCache& ids, Observation& o) {
  if (!f.rid_ble) return false;
  const uint8_t* odid;
  size_t n;
  if (!ble_find_odid(adv, len, &odid, &n)) return false;
  if (!rid(SourceType::REMOTEID_BLE, mac, odid, n, ids, now, o)) return false;
  o.t_ms = now;
  o.rssi_dbm = rssi;
  o.freq_khz = 2402000;  // advertising channel not reported
  return true;
}

void obs_from_lora_packet(PlanKind kind, uint32_t f, uint8_t sf, uint32_t bw_hz, const uint8_t* buf, size_t len,
                          float rssi, float snr, uint32_t now, Observation& o) {
  memset(&o, 0, sizeof o);
  o.t_ms = now;
  o.freq_khz = f;
  o.rssi_dbm = (int16_t)rssi;
  o.snr_db = (int8_t)snr;
  o.lora = {sf, bw_hz, 0};
  MeshHeader mh;
  LwHeader lw;
  if (kind == PlanKind::MESHTASTIC && parse_meshtastic(buf, len, mh)) {
    o.type = SourceType::MESHTASTIC;
    o.conf = Confidence::DECODED;
    format_mesh_id(mh.sender, o.id);
    snprintf(o.label, sizeof o.label, "Mesh %s", o.id);
  } else if (kind == PlanKind::LORAWAN && parse_lorawan(buf, len, lw)) {
    o.type = SourceType::LORAWAN;
    o.conf = Confidence::DECODED;
    format_lw_id(lw, o.id);
    snprintf(o.label, sizeof o.label, "LW %s", o.id);
  } else {
    o.type = SourceType::LORA_UNKNOWN;
    o.conf = Confidence::HEURISTIC;
    snprintf(o.id, sizeof o.id, "LORA-%u-SF%u", (unsigned)(f / 1000), (unsigned)sf);
    snprintf(o.label, sizeof o.label, "LoRa %u.%03u SF%u", (unsigned)(f / 1000), (unsigned)(f % 1000), (unsigned)sf);
  }
}
