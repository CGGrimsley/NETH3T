#pragma once
#include <cstdint>
// SY6970 charger / battery monitor.
namespace power {
bool init();
int8_t battery_pct();  // -1 unknown
bool charging();
}  // namespace power
