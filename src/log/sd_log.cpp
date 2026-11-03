#include "log/sd_log.h"
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <sys/time.h>
#include "board_pins.h"
#include "core/spi_bus.h"
#include "core/target_db.h"
#include "log/csv_format.h"
#include "log/serial_cmd.h"
#include "settings/settings.h"

namespace {
QueueHandle_t g_q = nullptr;
File g_file;
bool g_active = false;
char* g_buf = nullptr;  // 4 KB, PSRAM
constexpr size_t kBufSize = 4096;
size_t g_len = 0;
uint32_t g_last_flush = 0;
char g_path[24] = "";

void flush_locked() {
  if (!g_active || g_len == 0) { g_len = 0; return; }
  spi_bus::Guard g;
  size_t w = g_file.write((const uint8_t*)g_buf, g_len);
  g_file.flush();
  if (w != g_len) {  // card removed or full
    g_file.close();
    g_active = false;
    Serial.println("LOG SD write failed; SD logging stopped");
  }
  g_len = 0;
  g_last_flush = millis();
}

void append(const char* row, size_t n) {
  if (g_len + n + 1 > kBufSize) flush_locked();
  if (n + 1 > kBufSize) return;
  memcpy(g_buf + g_len, row, n);
  g_len += n;
  g_buf[g_len++] = '\n';
}

void task(void*) {
  Observation o;
  char row[320], iso[24];
  for (;;) {
    if (xQueueReceive(g_q, &o, pdMS_TO_TICKS(100)) == pdTRUE) {
      sd_log::iso_now(iso, sizeof iso);
      size_t n = csv_row(o, iso, row, sizeof row);
      if (n) {
        Serial.print("LOG,");
        Serial.println(row);
        if (g_active && settings::get().sd_log) append(row, n);
      }
    }
    if (millis() - g_last_flush >= 2000) flush_locked();
    serial_cmd::poll();
  }
}
}  // namespace

namespace sd_log {
bool init() {
  g_buf = (char*)heap_caps_malloc(kBufSize, MALLOC_CAP_SPIRAM);
  if (!g_buf) g_buf = (char*)malloc(kBufSize);
  g_q = xQueueCreate(32, sizeof(Observation));
  {
    spi_bus::Guard g;
    if (SD.begin(PIN_SD_CS, SPI, 10000000) && SD.cardType() != CARD_NONE) {
      if (!SD.exists("/neth3t")) SD.mkdir("/neth3t");
      for (unsigned i = 0; i < 1000; i++) {
        snprintf(g_path, sizeof g_path, "/neth3t/S%03u.csv", i);
        if (!SD.exists(g_path)) break;
      }
      g_file = SD.open(g_path, FILE_WRITE);
      if (g_file) {
        g_file.println(kCsvHeader);
        g_file.flush();
        g_active = true;
      }
    }
  }
  Serial.printf("LOG header %s\n", kCsvHeader);
  if (g_active) Serial.printf("LOG file %s\n", g_path);
  xTaskCreatePinnedToCore(task, "log", 6144, nullptr, 1, nullptr, 0);
  return g_active;
}

void write(const Observation& o) {
  if (is_test_id(o.id)) return;  // self-test targets are never logged
  if (g_q) xQueueSend(g_q, &o, 0);
}

void flush() { flush_locked(); }
bool active() { return g_active; }

void iso_now(char* out, size_t cap) {
  time_t t = time(nullptr);
  if (t < 1735689600) { if (cap) out[0] = '\0'; return; }  // before 2025-01-01: clock not set
  struct tm tm;
  gmtime_r(&t, &tm);
  strftime(out, cap, "%Y-%m-%dT%H:%M:%SZ", &tm);
}
}  // namespace sd_log
