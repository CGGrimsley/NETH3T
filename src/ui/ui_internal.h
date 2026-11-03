#pragma once
// Shared between the UI router and screen implementations.
#include <Arduino_GFX_Library.h>
#include "board_pins.h"
#include "core/target_db.h"
#include "ui/buttons.h"

struct Input {
  Btn btn;
  bool tap;
  int16_t x, y;
  int8_t swipe;  // -1 left, +1 right, 0 none
  bool home;     // touch home key
};

namespace ui_color {
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
constexpr uint16_t kRed = rgb(0xE5, 0x48, 0x4D);
constexpr uint16_t kAmber = rgb(0xF5, 0xA5, 0x24);
constexpr uint16_t kGrey = rgb(0x8B, 0x8D, 0x98);
constexpr uint16_t kDim = rgb(0x40, 0x42, 0x48);
constexpr uint16_t kBg = rgb(0x0B, 0x0D, 0x12);
constexpr uint16_t kPanel = rgb(0x1A, 0x1D, 0x24);
constexpr uint16_t kText = rgb(0xED, 0xEE, 0xF0);
constexpr uint16_t kGreen = rgb(0x30, 0xA4, 0x6C);
constexpr uint16_t kBlue = rgb(0x3E, 0x63, 0xDD);
}  // namespace ui_color

constexpr int kStatusH = 24;

// Bottom tab bar on Targets / Spectrum / Settings (not on Hunt).
constexpr int kTabH = 44, kTabY = TFT_H - kTabH;

// Touch "Back" button, top-right under the status bar, on every non-home screen.
constexpr int kBackX = TFT_W - 62, kBackY = kStatusH + 2, kBackW = 58, kBackH = 30;
inline void draw_back(Arduino_Canvas* c) {
  c->fillRoundRect(kBackX, kBackY, kBackW, kBackH, 6, ui_color::kPanel);
  c->drawRoundRect(kBackX, kBackY, kBackW, kBackH, 6, ui_color::kGrey);
  c->setTextSize(2);
  c->setTextColor(ui_color::kText);
  c->setCursor(kBackX + 6, kBackY + 8);
  c->print("Back");
}
inline bool back_pressed(const Input& in) {
  if (in.btn == Btn::BACK || in.swipe != 0) return true;  // BOOT button or any swipe
  return in.tap && in.x >= kBackX - 6 && in.y < kBackY + kBackH + 6;
}

uint16_t target_color(const Target& t);  // by type/confidence
char target_icon(const Target& t);       // D/R/L/W
void format_freq(const Target& t, char* out, size_t cap);
uint16_t dim(uint16_t c);                // 50 % brightness
bool is_lora_node(SourceType t);

namespace ui_targets {
void draw(Arduino_Canvas* c, const Target* t, size_t n);
void input(const Input& in, const Target* t, size_t n);
}
namespace ui_hunt {
void draw(Arduino_Canvas* c, const Target& t, uint32_t now);
void input(const Input& in, const Target& t);
}
// Spectrum draws in landscape; the router rotates canvas and touch for it.
constexpr uint8_t kSpectrumRotation = 1;  // flip to 3 if the picture is upside down
namespace ui_spectrum {
void enter();   // sets the scanner focus for the viewed range
void leave();   // clears the scanner focus; called on every exit
void draw(Arduino_Canvas* c);
void input(const Input& in);
}
namespace ui_selftest {
void draw(Arduino_Canvas* c);
void input(const Input& in);
}
namespace ui_settings {
void draw(Arduino_Canvas* c);
void input(const Input& in);
}
