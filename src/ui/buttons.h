#pragma once
#include <cstdint>
enum class Btn : uint8_t { NONE, BACK, UP, DOWN, UP_LONG, DOWN_LONG };
// BOOT=back, 12=up (long: lock/unlock), 16=down (long: settings).
namespace buttons {
void init();
Btn poll();  // 30 ms debounce; short press fires on release, long press at 700 ms
}
