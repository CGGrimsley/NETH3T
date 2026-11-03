#include "core/db_task.h"
#include <Arduino.h>
#include <new>
#include "core/health.h"

namespace {
QueueHandle_t g_q = nullptr;
SemaphoreHandle_t g_mx = nullptr;
TargetDb* g_db = nullptr;
volatile uint32_t g_dropped = 0;
db::Sink g_sink = nullptr;

struct Lock {
  Lock() { xSemaphoreTake(g_mx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(g_mx); }
};

void task(void*) {
  Observation o;
  uint32_t last_tick = 0;
  for (;;) {
    health::beat(health::DB);
    if (xQueueReceive(g_q, &o, pdMS_TO_TICKS(200)) == pdTRUE) {
      { Lock l; g_db->ingest(o); }
      if (g_sink) g_sink(o);
    }
    uint32_t now = millis();
    if (now - last_tick >= 1000) {
      Lock l;
      g_db->tick(now);
      last_tick = now;
    }
  }
}
}  // namespace

namespace db {
void start() {
  void* mem = heap_caps_malloc(sizeof(TargetDb), MALLOC_CAP_SPIRAM);
  if (!mem) mem = malloc(sizeof(TargetDb));
  g_db = new (mem) TargetDb();
  g_mx = xSemaphoreCreateMutex();
  g_q = xQueueCreate(64, sizeof(Observation));
  xTaskCreatePinnedToCore(task, "db", 6144, nullptr, 3, nullptr, 0);
}
bool post(const Observation& o) {
  if (!g_q || xQueueSend(g_q, &o, 0) != pdTRUE) { g_dropped++; return false; }
  return true;
}
size_t snapshot(Target* out, size_t max) { Lock l; return g_db->snapshot(out, max); }
bool lock(SourceType t, const char* id) { Lock l; return g_db->lock(t, id); }
void unlock() { Lock l; g_db->unlock(); }
bool locked(Target* out) {
  Lock l;
  const Target* t = g_db->locked();
  if (!t) return false;
  *out = *t;
  return true;
}
void reset_peak() {
  Lock l;
  const Target* t = g_db->locked();
  if (t) g_db->reset_peak(t->type, t->id);
}
uint32_t dropped() { return g_dropped; }
void mark_alerted(SourceType t, const char* id) {
  Lock l;
  Target* p = g_db->find(t, id);
  if (p) p->alerted = true;
}
bool remove(SourceType t, const char* id) {
  Lock l;
  return g_db->remove(t, id);
}
void set_sink(Sink s) { g_sink = s; }
}  // namespace db
