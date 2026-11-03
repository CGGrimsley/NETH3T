#pragma once
#include <cstdint>
// CST226SE capacitive touch.
namespace touch {
bool init();
bool read(int16_t& x, int16_t& y);  // true while touched
bool home_pressed();  // the printed circle key under the screen; true once per press
}
