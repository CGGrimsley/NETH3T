#include "radio/wifi_sniffer.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <cstring>
#include "core/db_task.h"
#include "core/health.h"
#include "detect/drone_vendors.h"
#include "detect/id_cache.h"
#include "detect/wifi_frames.h"
#include "radio/rid_common.h"
#include "settings/settings.h"

namespace {
struct Frame {
  uint16_t len;
  int8_t rssi;
  uint8_t ch;
  uint8_t data[512];
};
QueueHandle_t g_q = nullptr;
volatile uint8_t g_pinned = 0;
volatile uint8_t g_ch = 1;
IdCache g_ids;
const uint8_t kHop[] = {1, 6, 11, 2, 7, 12, 3, 8, 13, 4, 9, 5, 10};

void on_pkt(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT || !g_q) return;
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
  uint8_t fc0 = p->payload[0];
  if (fc0 != 0x80 && fc0 != 0x50 && fc0 != 0xD0) return;  // beacon, probe resp, action
  Frame f;
  uint16_t len = p->rx_ctrl.sig_len;
  len = len > 4 ? len - 4 : 0;  // strip FCS
  if (len > sizeof f.data) len = sizeof f.data;
  f.len = len;
  f.rssi = (int8_t)p->rx_ctrl.rssi;
  f.ch = (uint8_t)p->rx_ctrl.channel;
  memcpy(f.data, p->payload, len);
  xQueueSend(g_q, &f, 0);
}

void handle(const Frame& f) {
  WifiParse w;
  if (!wifi_parse_mgmt(f.data, f.len, w)) return;
  const Settings& s = settings::get();
  uint32_t now = millis();
  Observation o;
  switch (w.kind) {
    case WifiKind::ODID_BEACON:
    case WifiKind::ODID_NAN:
      if (!s.en_rid_wifi) return;
      if (!make_rid_observation(SourceType::REMOTEID_WIFI, w.src, w.odid, w.odid_len, g_ids, now, o))
        return;
      break;
    case WifiKind::DJI_LEGACY:
      if (!s.en_rid_wifi) return;
      memset(&o, 0, sizeof o);
      o.type = SourceType::DJI_LEGACY;
      o.conf = Confidence::FINGERPRINT;
      format_mac(w.src, o.id);
      snprintf(o.label, sizeof o.label, "DJI DroneID");
      snprintf(o.vendor, sizeof o.vendor, "DJI");
      break;
    case WifiKind::PLAIN_BEACON: {
      if (!s.en_drone_wifi) return;
      const char* v = drone_vendor_by_oui(w.src);
      if (!v) v = drone_vendor_by_ssid(w.ssid);
      if (!v) return;
      memset(&o, 0, sizeof o);
      o.type = SourceType::DRONE_WIFI;
      o.conf = Confidence::HEURISTIC;
      format_mac(w.src, o.id);
      snprintf(o.label, sizeof o.label, "%s", w.ssid[0] ? w.ssid : o.id);
      snprintf(o.vendor, sizeof o.vendor, "%s", v);
      break;
    }
    default:
      return;
  }
  o.t_ms = now;
  o.rssi_dbm = f.rssi;
  o.freq_khz = 2407000 + (uint32_t)f.ch * 5000;
  db::post(o);
}

void parse_task(void*) {
  Frame f;
  for (;;)
    if (xQueueReceive(g_q, &f, portMAX_DELAY) == pdTRUE) handle(f);
}

void hop_task(void*) {
  size_t i = 0;
  for (;;) {
    health::beat(health::WIFI);
    uint8_t ch = g_pinned ? g_pinned : kHop[i++ % sizeof kHop];
    if (ch != g_ch) {
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      g_ch = ch;
    }
    vTaskDelay(pdMS_TO_TICKS(g_pinned ? 100 : (ch == 6 ? 250 : 120)));
  }
}
}  // namespace

namespace wifi_sniffer {
void start() {
  g_q = xQueueCreate(16, sizeof(Frame));
  WiFi.mode(WIFI_STA);  // station mode, never associates; needed for promiscuous RX
  WiFi.disconnect();
  wifi_promiscuous_filter_t filt = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(on_pkt);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  xTaskCreatePinnedToCore(parse_task, "wifi_parse", 6144, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(hop_task, "wifi_hop", 3072, nullptr, 2, nullptr, 0);
}
void pin_channel(uint8_t ch) { g_pinned = ch <= 13 ? ch : 0; }
uint8_t channel() { return g_ch; }
}  // namespace wifi_sniffer
