#include "radio/ble_scanner.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "core/db_task.h"
#include "detect/ble_adv.h"
#include "detect/id_cache.h"
#include "radio/rid_common.h"
#include "settings/settings.h"

namespace {
IdCache g_ids;
NimBLEScan* g_scan = nullptr;
constexpr uint16_t kIntervalMs = 1000, kWindowMs = 300;

class Callbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    if (!settings::get().en_rid_ble) return;
    const std::vector<uint8_t>& p = d->getPayload();
    const uint8_t* odid;
    size_t n;
    if (!ble_find_odid(p.data(), p.size(), &odid, &n)) return;
    // NimBLE stores addresses little-endian; present them MSB first.
    const uint8_t* le = d->getAddress().getVal();
    uint8_t mac[6] = {le[5], le[4], le[3], le[2], le[1], le[0]};
    Observation o;
    if (!make_rid_observation(SourceType::REMOTEID_BLE, mac, odid, n, g_ids, millis(), o)) return;
    o.rssi_dbm = d->getRSSI();
    o.freq_khz = 2402000;  // advertising channel not reported
    db::post(o);
  }
};
Callbacks g_cb;
}  // namespace

namespace ble_scanner {
void start() {
  NimBLEDevice::init("");
  g_scan = NimBLEDevice::getScan();
  g_scan->setScanCallbacks(&g_cb, true);  // want duplicates: every advert updates RSSI
  g_scan->setActiveScan(false);           // passive: never transmits scan requests
  g_scan->setDuplicateFilter(false);
  g_scan->setMaxResults(0);               // don't store results
#if CONFIG_BT_NIMBLE_EXT_ADV
  g_scan->setPhy(NimBLEScan::SCAN_ALL);   // 1M + coded (Long Range) PHYs
#endif
  g_scan->setInterval(kIntervalMs);
  g_scan->setWindow(kWindowMs);
  g_scan->start(0, false, true);  // duration 0 = scan forever
}
void set_continuous(bool on) {
  if (!g_scan) return;
  g_scan->stop();
  g_scan->setWindow(on ? kIntervalMs : kWindowMs);
  g_scan->start(0, false, true);
}
}  // namespace ble_scanner
