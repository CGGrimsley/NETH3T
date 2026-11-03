#pragma once
#include <Arduino_GFX_Library.h>
// ST7796 222x480 with a full-frame PSRAM canvas, flushed in 40-row strips so
// the shared SPI bus is never held for long.
namespace display {
bool init();
Arduino_Canvas* canvas();
void flush();
void set_brightness(uint8_t level);  // 0-16
}  // namespace display
