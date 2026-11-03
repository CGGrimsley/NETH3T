#include "selftest/selftest.h"
#include <Arduino.h>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include "alert/alert_policy.h"
#include "alert/audio.h"
#include "alert/vibe.h"
#include "core/boot.h"
#include "core/db_task.h"
#include "detect/obs_build.h"
#include "detect/rc_fingerprint.h"
#include "log/csv_format.h"
#include "log/sd_log.h"
#include "radio/ble_scanner.h"
#include "radio/lora_scanner.h"
#include "radio/wifi_sniffer.h"
#include "selftest/vectors.h"

namespace {
SemaphoreHandle_t g_mx = nullptr;
volatile bool g_running = false;
const char* volatile g_progress = "idle";

StItem g_items[selftest::kItems] = {
    {"H1", "Boot peripherals", StResult::PENDING, ""},  {"H2", "SD card", StResult::PENDING, ""},
    {"H3", "LR1121 version", StResult::PENDING, ""},    {"H4", "LR1121 RSSI sweeps", StResult::PENDING, ""},
    {"H5", "LR1121 CAD", StResult::PENDING, ""},        {"H6", "LR1121 error rate", StResult::PENDING, ""},
    {"H7", "Wi-Fi receive", StResult::PENDING, ""},     {"H8", "BLE receive", StResult::PENDING, ""},
    {"P1", "RemoteID WiFi beacon", StResult::PENDING, ""}, {"P2", "RemoteID WiFi NAN", StResult::PENDING, ""},
    {"P3", "RemoteID BLE merge", StResult::PENDING, ""},   {"P4", "Drone-maker WiFi", StResult::PENDING, ""},
    {"P5", "ELRS fingerprint", StResult::PENDING, ""},     {"P6", "Meshtastic", StResult::PENDING, ""},
    {"P7", "LoRaWAN", StResult::PENDING, ""},              {"P8", "Alert policy", StResult::PENDING, ""},
    {"P9", "CSV log row", StResult::PENDING, ""},          {"M1", "Speaker + vibration", StResult::PENDING, ""},
};

void set(size_t i, StResult r, const char* fmt, ...) {
  char buf[40];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  xSemaphoreTake(g_mx, portMAX_DELAY);
  g_items[i].r = r;
  memcpy(g_items[i].detail, buf, sizeof buf);
  xSemaphoreGive(g_mx);
  Serial.printf("SELFTEST,%s,%s,%s\n", g_items[i].id, selftest::result_name(r), buf);
}

void prefix(Observation& o) {
  char id[24];
  snprintf(id, sizeof id, "TEST-%s", o.id);
  memcpy(o.id, id, sizeof id);
}

// Posts a test observation and waits up to 1 s for it to reach the table.
// On timeout the (late) target is removed so nothing leaks; the end-of-run
// sweep catches anything that arrives later still.
bool inject(Observation& o, Target* out) {
  prefix(o);
  if (!db::post(o)) return false;
  for (int i = 0; i < 20; i++) {
    vTaskDelay(pdMS_TO_TICKS(50));
    if (db::get(o.type, o.id, out)) return true;
  }
  db::remove(o.type, o.id);
  return false;
}

// Removes every TEST- target still in the table.
void remove_all_test_targets() {
  vTaskDelay(pdMS_TO_TICKS(300));  // let any queued test observation land first
  Target* snap = (Target*)heap_caps_malloc(sizeof(Target) * TargetDb::kCapacity, MALLOC_CAP_SPIRAM);
  if (!snap) return;
  size_t n = db::snapshot(snap, TargetDb::kCapacity);
  for (size_t i = 0; i < n; i++)
    if (is_test_id(snap[i].id)) db::remove(snap[i].type, snap[i].id);
  heap_caps_free(snap);
}

bool near(double a, double b) { return fabs(a - b) <= 1e-6; }

const DetectFlags kAll = {true, true, true};
// Results kept for P8/P9; the *_ok flags stop a stale earlier run from passing them.
Target g_t1, g_t4, g_t6;
Observation g_o1;
bool g_t1_ok, g_t4_ok, g_t6_ok;
// Large ELRS check buffers live here, not on the 16 KB task stack.
RcFingerprint g_fp;
Observation g_fp_out[4];

void check_rid_wifi(size_t item, void (*vec)(Vec&)) {
  Vec v;
  vec(v);
  IdCache ids;
  Observation o;
  Target t;
  if (!obs_from_wifi(v.data, v.len, -50, 6, millis(), kAll, ids, o)) { set(item, StResult::FAIL, "frame not decoded"); return; }
  Observation copy = o;
  if (!inject(o, &t)) { set(item, StResult::FAIL, "not seen in table (queue full?)"); return; }
  bool ok = t.odid.has_basic && strcmp(t.odid.uas_id, kVecSerialWifi) == 0 && t.odid.loc_valid &&
            near(t.odid.lat, kVecDroneLat) && near(t.odid.lon, kVecDroneLon) && t.odid.op_valid &&
            near(t.odid.op_lat, kVecPilotLat) && near(t.odid.op_lon, kVecPilotLon);
  if (ok) set(item, StResult::PASS, "serial, drone + pilot position OK");
  else set(item, StResult::FAIL, "wrong fields in target");
  if (item == 8 && ok) { g_t1 = t; g_o1 = copy; prefix(g_o1); g_t1_ok = true; }
  db::remove(t.type, t.id);
}

void check_ble() {
  Vec v;
  IdCache ids;
  Observation a, b;
  Target t;
  vec_rid_ble_basic(v);
  bool ok_a = obs_from_ble(kVecBleMac, v.data, v.len, -60, millis(), kAll, ids, a);
  vec_rid_ble_location(v);
  bool ok_b = obs_from_ble(kVecBleMac, v.data, v.len, -60, millis(), kAll, ids, b);
  if (!ok_a || !ok_b) { set(10, StResult::FAIL, "advert not decoded"); return; }
  prefix(a);
  if (!db::post(a)) { set(10, StResult::FAIL, "queue full"); return; }
  if (!inject(b, &t)) {
    db::remove(a.type, a.id);
    set(10, StResult::FAIL, "not seen in table (queue full?)");
    return;
  }
  vTaskDelay(pdMS_TO_TICKS(100));
  db::get(t.type, t.id, &t);
  char want[24];
  snprintf(want, sizeof want, "TEST-%s", kVecSerialBle);
  bool ok = t.odid.has_basic && t.odid.loc_valid && near(t.odid.lat, kVecBleLat) &&
            near(t.odid.lon, kVecBleLon) && strcmp(t.id, want) == 0;
  if (ok) set(10, StResult::PASS, "two adverts merged into one target");
  else set(10, StResult::FAIL, "adverts did not merge");
  db::remove(t.type, t.id);
}

void check_tello() {
  Vec v;
  vec_tello_beacon(v);
  IdCache ids;
  Observation o;
  Target t;
  if (!obs_from_wifi(v.data, v.len, -55, 1, millis(), kAll, ids, o)) { set(11, StResult::FAIL, "SSID not matched"); return; }
  if (!inject(o, &t)) { set(11, StResult::FAIL, "not seen in table"); return; }
  bool ok = t.type == SourceType::DRONE_WIFI && strcmp(t.vendor, "Ryze/DJI Tello") == 0;
  if (ok) set(11, StResult::PASS, "TELLO SSID -> Ryze/DJI Tello");
  else set(11, StResult::FAIL, "wrong vendor");
  if (ok) { g_t4 = t; g_t4_ok = true; }
  db::remove(t.type, t.id);
}

void check_elrs() {
  g_fp = RcFingerprint();
  CadHit h[8];
  uint32_t now = millis();
  size_t n = vec_elrs_hits(h, 8, now - 700);
  for (size_t i = 0; i < n; i++) g_fp.add_cad(h[i]);
  Target t;
  if (g_fp.evaluate(now, g_fp_out, 4) != 1) { set(12, StResult::FAIL, "hop pattern not classified"); return; }
  if (!inject(g_fp_out[0], &t)) { set(12, StResult::FAIL, "not seen in table"); return; }
  if (strcmp(t.id, "TEST-ELRS900-SF7") == 0 && t.lora.rate_hz == 100) set(12, StResult::PASS, "ELRS900-SF7 ~100Hz");
  else set(12, StResult::FAIL, "wrong class/rate");
  db::remove(t.type, t.id);
}

void check_lora(size_t item, void (*vec)(Vec&), PlanKind kind, uint32_t f, uint8_t sf, uint32_t bw,
                const char* want) {
  Vec v;
  vec(v);
  Observation o;
  Target t;
  obs_from_lora_packet(kind, f, sf, bw, v.data, v.len, -90, 5, millis(), o);
  if (!inject(o, &t)) { set(item, StResult::FAIL, "not seen in table"); return; }
  char expect[24];
  snprintf(expect, sizeof expect, "TEST-%s", want);
  bool ok = strcmp(t.id, expect) == 0;
  if (ok) set(item, StResult::PASS, "%s", want);
  else set(item, StResult::FAIL, "got %s", t.id);
  if (item == 13 && ok) { g_t6 = t; g_t6_ok = true; }
  db::remove(t.type, t.id);
}

void check_alerts() {
  if (!g_t1_ok || !g_t4_ok || !g_t6_ok) { set(15, StResult::FAIL, "depends on P1/P4/P6"); return; }
  AlertLevel a = alert_for(g_t1, 2), b = alert_for(g_t4, 2), c = alert_for(g_t6, 2);
  if (a == AlertLevel::ALARM && b == AlertLevel::NOTICE && c == AlertLevel::NONE)
    set(15, StResult::PASS, "drone ALARM, wifi NOTICE, mesh none");
  else
    set(15, StResult::FAIL, "levels %d/%d/%d", (int)a, (int)b, (int)c);
}

void check_csv() {
  if (!g_t1_ok) { set(16, StResult::FAIL, "depends on P1"); return; }
  char row[320];
  size_t n = csv_row(g_o1, "", row, sizeof row);
  if (n && strstr(row, kVecSerialWifi) && strstr(row, "35.7796000")) set(16, StResult::PASS, "row has serial + 7-dp lat");
  else set(16, StResult::FAIL, "row malformed");
}

bool sweep_sane(uint8_t band, char* why, size_t cap) {
  lora_scanner::SweepRow r;
  if (!lora_scanner::latest_sweep(band, r)) { snprintf(why, cap, "band %u: no sweep", band); return false; }
  if (millis() - r.t_ms > 10000) { snprintf(why, cap, "band %u: sweep stale", band); return false; }
  bool varied = false;
  for (uint8_t i = 0; i < r.n; i++) {
    if (r.dbm[i] < -135 || r.dbm[i] > -20) { snprintf(why, cap, "band %u: %d dBm", band, r.dbm[i]); return false; }
    if (r.dbm[i] != r.dbm[0]) varied = true;
  }
  if (!varied) { snprintf(why, cap, "band %u: stuck at %d", band, r.dbm[0]); return false; }
  return true;
}

void run(void*) {
  uint32_t t0 = millis();
  lora_scanner::Stats s0 = lora_scanner::stats();
  uint32_t frames0 = wifi_sniffer::frames(), adverts0 = ble_scanner::adverts();
  Serial.println("SELFTEST,START");
  g_t1_ok = g_t4_ok = g_t6_ok = false;

  g_progress = "Hardware";
  if (!g_boot.display || !g_boot.lora) set(0, StResult::FAIL, "display %s, LoRa %s", g_boot.display ? "ok" : "FAIL", g_boot.lora ? "ok" : "FAIL");
  else if (!g_boot.pmu || !g_boot.touch || !g_boot.rtc) set(0, StResult::WARN, "PMU %s touch %s RTC %s", g_boot.pmu ? "ok" : "x", g_boot.touch ? "ok" : "x", g_boot.rtc ? "ok" : "x");
  else set(0, StResult::PASS, "all peripherals OK");
  if (sd_log::active()) set(1, StResult::PASS, "logging to SD");
  else set(1, StResult::WARN, "no SD card / not writable");
  if (lora_scanner::ok() && strcmp(lora_scanner::version(), "-") != 0) set(2, StResult::PASS, "%.38s", lora_scanner::version());
  else set(2, StResult::FAIL, "LR1121 not responding (err %d)", g_boot.lora_err);

  g_progress = "Detectors";
  check_rid_wifi(8, vec_rid_beacon);
  check_rid_wifi(9, vec_rid_nan);
  check_ble();
  check_tello();
  check_elrs();
  check_lora(13, vec_mesh_packet, PlanKind::MESHTASTIC, 906875, 11, 250000, "!5e1f7e57");
  check_lora(14, vec_lorawan_uplink, PlanKind::LORAWAN, 903900, 7, 125000, "DA:26011BDA");
  check_alerts();
  check_csv();

  g_progress = "Speaker + vibration";
  audio::chirp();
  vibe::double_pulse();
  set(17, StResult::CHECK, "did it chirp and buzz?");

  // Radio liveness: wait for counters to move (Wi-Fi/BLE 5 s, LoRa up to 12 s).
  g_progress = "Radios (up to 12 s)";
  bool wifi = false, ble = false, cad = false, rssi = false;
  char why[40] = "";
  while (millis() - t0 < 12000 && !(wifi && ble && cad && rssi)) {
    if (!wifi && wifi_sniffer::frames() != frames0) { wifi = true; set(6, StResult::PASS, "%lu frames", (unsigned long)(wifi_sniffer::frames() - frames0)); }
    if (!ble && ble_scanner::adverts() != adverts0) { ble = true; set(7, StResult::PASS, "%lu adverts", (unsigned long)(ble_scanner::adverts() - adverts0)); }
    lora_scanner::Stats s = lora_scanner::stats();
    if (!cad && s.cad_runs != s0.cad_runs) { cad = true; set(4, StResult::PASS, "%lu CAD runs", (unsigned long)(s.cad_runs - s0.cad_runs)); }
    if (!rssi) rssi = sweep_sane(0, why, sizeof why) && sweep_sane(1, why, sizeof why) && sweep_sane(2, why, sizeof why);
    if (rssi && g_items[3].r == StResult::PENDING) set(3, StResult::PASS, "all 3 bands sane");
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  if (!wifi) set(6, StResult::WARN, "no Wi-Fi frames in 12 s");
  if (!ble) set(7, StResult::WARN, "no BLE adverts in 12 s");
  if (!cad) set(4, StResult::FAIL, "no CAD completed");
  if (!rssi) set(3, StResult::FAIL, "%s", why);
  lora_scanner::Stats s1 = lora_scanner::stats();
  uint32_t ops = (s1.cad_runs - s0.cad_runs) + (s1.sweeps - s0.sweeps);
  uint32_t errs = s1.errors - s0.errors;
  if (ops == 0) set(5, StResult::FAIL, "no radio operations");
  else if (errs * 2 >= ops) set(5, StResult::FAIL, "%lu errors / %lu ops", (unsigned long)errs, (unsigned long)ops);
  else if (errs * 20 >= ops) set(5, StResult::WARN, "%lu errors / %lu ops", (unsigned long)errs, (unsigned long)ops);
  else set(5, StResult::PASS, "%lu errors / %lu ops", (unsigned long)errs, (unsigned long)ops);

  remove_all_test_targets();
  int pass = 0, warn = 0, fail = 0;
  for (const StItem& it : g_items) {
    if (it.r == StResult::PASS) pass++;
    else if (it.r == StResult::WARN) warn++;
    else if (it.r == StResult::FAIL) fail++;
  }
  Serial.printf("SELFTEST,SUMMARY,pass=%d,warn=%d,fail=%d,stack_free=%u\n", pass, warn, fail,
                (unsigned)uxTaskGetStackHighWaterMark(nullptr));
  g_progress = "done";
  g_running = false;
  vTaskDelete(nullptr);
}
}  // namespace

namespace selftest {
bool start() {
  if (!g_mx) g_mx = xSemaphoreCreateMutex();
  if (g_running) { Serial.println("SELFTEST,BUSY,already running"); return false; }
  g_running = true;
  for (StItem& it : g_items) { it.r = StResult::PENDING; it.detail[0] = '\0'; }
  g_progress = "starting";
  if (xTaskCreatePinnedToCore(run, "selftest", 16384, nullptr, 1, nullptr, 0) != pdPASS) {
    g_running = false;
    return false;
  }
  return true;
}
bool running() { return g_running; }
size_t items(StItem* out, size_t max) {
  if (!g_mx) return 0;
  size_t n = max < kItems ? max : kItems;
  xSemaphoreTake(g_mx, portMAX_DELAY);
  memcpy(out, g_items, n * sizeof(StItem));
  xSemaphoreGive(g_mx);
  return n;
}
const char* progress() { return g_progress; }
const char* result_name(StResult r) {
  switch (r) {
    case StResult::PASS: return "PASS";
    case StResult::WARN: return "WARN";
    case StResult::FAIL: return "FAIL";
    case StResult::CHECK: return "CHECK";
    default: return "...";
  }
}
}  // namespace selftest
