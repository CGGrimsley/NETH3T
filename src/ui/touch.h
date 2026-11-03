#pragma once
#include <cstdint>
// CST226SE capacitive touch.
namespace touch {
bool init();
bool read(int16_t& x, int16_t& y);  // true while touched
}
