#include "log/serial_cmd.h"
#include <Arduino.h>
#include <Wire.h>
#include <RtcDrv.hpp>
#include <sys/time.h>
#include "board_pins.h"
#include "core/db_task.h"
#include "core/health.h"
#include "log/csv_format.h"
#include "radio/lora_scanner.h"
#include "settings/settings.h"

namespace {
SensorPCF85063 g_rtc;
bool g_rtc_ok = false;
char g_line[64];
size_t g_n = 0;

void set_clock(time_t t) {
  timeval tv = {t, 0};
  settimeofday(&tv, nullptr);
}

void cmd_time(const char* arg) {
  long long t = atoll(arg);
  if (t < 1735689600LL) { Serial.println("ERR time: expected unix seconds (>= 2025)"); return; }
  set_clock((time_t)t);
  if (g_rtc_ok) {
    struct tm tm;
    time_t tt = (time_t)t;
    gmtime_r(&tt, &tm);
    g_rtc.setDateTime(RTC_DateTime(tm));
  }
  Serial.printf("OK time set%s\n", g_rtc_ok ? " (RTC updated)" : " (no RTC)");
}

void cmd_region(const char* arg) {
  Settings& s = settings::get();
  if (!strcasecmp(arg, "fcc")) s.region = Region::FCC;
  else if (!strcasecmp(arg, "eu")) s.region = Region::EU;
  else { Serial.println("ERR region: fcc|eu"); return; }
  settings::save();
  lora_scanner::set_region(s.region);
  Serial.printf("OK region %s\n", arg);
}

void cmd_dump() {
  static Target* snap = nullptr;
  if (!snap) snap = (Target*)heap_caps_malloc(sizeof(Target) * TargetDb::kCapacity, MALLOC_CAP_SPIRAM);
  if (!snap) { Serial.println("ERR dump: no memory"); return; }
  size_t n = db::snapshot(snap, TargetDb::kCapacity);
  uint32_t now = millis();
  Serial.printf("DUMP %u targets\n", (unsigned)n);
  for (size_t i = 0; i < n; i++) {
    const Target& t = snap[i];
    Serial.printf("DUMP,%s,%s,%s,%s,%lu,%.1f,%d,%lu,%lus%s", source_type_name(t.type), conf_name(t.conf), t.id,
                  t.label, (unsigned long)t.freq_khz, t.ema_rssi, t.peak_rssi, (unsigned long)t.hits,
                  (unsigned long)((now - t.last_ms) / 1000), t.stale ? ",stale" : "");
    if (t.odid.has_loc && t.odid.loc_valid) Serial.printf(",drone=%.7f;%.7f", t.odid.lat, t.odid.lon);
    if (t.odid.has_sys && t.odid.op_valid) Serial.printf(",pilot=%.7f;%.7f", t.odid.op_lat, t.odid.op_lon);
    Serial.println();
  }
}

void cmd_stats() {
  lora_scanner::Stats s = lora_scanner::stats();
  Serial.printf("STATS cad_hits=%lu rx_pkts=%lu errors=%lu suspended=%lu dropped=%lu restarts=%lu heap=%lu psram=%lu plan=%s\n",
                (unsigned long)s.cad_hits, (unsigned long)s.rx_pkts, (unsigned long)s.errors,
                (unsigned long)s.suspended, (unsigned long)db::dropped(), (unsigned long)health::restarts(),
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram(), lora_scanner::current_plan());
}

void cmd_sweep() {
  static const char* const kBands[3] = {"915/868", "433", "2400"};
  for (uint8_t b = 0; b < 3; b++) {
    lora_scanner::SweepRow r;
    if (!lora_scanner::latest_sweep(b, r)) { Serial.printf("SWEEP %s none\n", kBands[b]); continue; }
    Serial.printf("SWEEP %s f0=%lu step=%lu n=%u floor=%d age=%lums dbm=", kBands[b], (unsigned long)r.f0_khz,
                  (unsigned long)r.step_khz, r.n, r.floor_dbm, (unsigned long)(millis() - r.t_ms));
    for (uint8_t i = 0; i < r.n; i++) Serial.printf(i ? ",%d" : "%d", r.dbm[i]);
    Serial.println();
  }
}

void run(char* line) {
  char* arg = strchr(line, ' ');
  if (arg) { *arg++ = '\0'; while (*arg == ' ') arg++; } else { arg = line + strlen(line); }
  if (!strcmp(line, "time")) cmd_time(arg);
  else if (!strcmp(line, "region")) cmd_region(arg);
  else if (!strcmp(line, "dump")) cmd_dump();
  else if (!strcmp(line, "stats")) cmd_stats();
  else if (!strcmp(line, "sweep")) cmd_sweep();
  else if (line[0]) Serial.println("ERR commands: time <unix> | region fcc|eu | dump | stats | sweep");
}
}  // namespace

namespace serial_cmd {
bool rtc_init() {
  setenv("TZ", "UTC0", 1);  // device clock and logs are UTC
  tzset();
  g_rtc_ok = g_rtc.begin(Wire, PIN_I2C_SDA, PIN_I2C_SCL);
  if (!g_rtc_ok) return false;
  RTC_DateTime d = g_rtc.getDateTime();
  if (d.getYear() < 2025) return true;  // RTC present but never set
  struct tm tm = {};
  tm.tm_year = d.getYear() - 1900;
  tm.tm_mon = d.getMonth() - 1;
  tm.tm_mday = d.getDay();
  tm.tm_hour = d.getHour();
  tm.tm_min = d.getMinute();
  tm.tm_sec = d.getSecond();
  set_clock(mktime(&tm));
  return true;
}

void poll() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      g_line[g_n] = '\0';
      run(g_line);
      g_n = 0;
    } else if (g_n + 1 < sizeof g_line) {
      g_line[g_n++] = c;
    }
  }
}
}  // namespace serial_cmd
