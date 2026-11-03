#pragma once
#include <cstddef>
#include <cstdint>
// Receive-only on-device self-test (spec 2026-11-03-selftest-and-spectrum §2).
enum class StResult : uint8_t { PENDING, PASS, WARN, FAIL, CHECK };
struct StItem { const char* id; const char* name; StResult r; char detail[40]; };

namespace selftest {
constexpr size_t kItems = 18;
bool start();      // false if already running; runs in its own task
bool running();
size_t items(StItem* out, size_t max);  // snapshot for the screen
const char* progress();                 // current check, or "done"
const char* result_name(StResult r);
}
