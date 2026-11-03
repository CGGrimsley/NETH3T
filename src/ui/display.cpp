#include "ui/display.h"
#include <SPI.h>
#include "board_pins.h"
#include "core/spi_bus.h"

namespace {
Arduino_DataBus* g_bus = nullptr;
Arduino_GFX* g_gfx = nullptr;
Arduino_Canvas* g_canvas = nullptr;
uint8_t g_level = 0;
constexpr int kStripRows = 40;
}  // namespace

namespace display {
bool init() {
  spi_bus::Guard g;
  g_bus = new Arduino_HWSPI(PIN_TFT_DC, PIN_TFT_CS, PIN_SPI_SCK, PIN_SPI_MOSI, PIN_SPI_MISO, &SPI, true);
  g_gfx = new Arduino_ST7796(g_bus, PIN_TFT_RST, 0, true, TFT_W, TFT_H, TFT_COL_OFFSET, 0, 0, 0);
  if (!g_gfx->begin()) return false;
  g_gfx->fillScreen(BLACK);
  g_canvas = new Arduino_Canvas(TFT_W, TFT_H, g_gfx);
  if (!g_canvas->begin(GFX_SKIP_OUTPUT_BEGIN)) return false;
  pinMode(PIN_TFT_BL, OUTPUT);
  set_brightness(12);
  return true;
}

Arduino_Canvas* canvas() { return g_canvas; }

void flush() {
  if (!g_canvas) return;
  uint16_t* fb = g_canvas->getFramebuffer();
  for (int y = 0; y < TFT_H; y += kStripRows) {
    int rows = min(kStripRows, TFT_H - y);
    spi_bus::Guard g;
    g_gfx->draw16bitRGBBitmap(0, y, fb + y * TFT_W, TFT_W, rows);
  }
}

// Board V1.1 backlight driver: 16 levels selected by pulse count (LilyGO AdjustBacklight).
void set_brightness(uint8_t value) {
  const uint8_t steps = 16;
  if (value > steps) value = steps;
  if (value == 0) {
    digitalWrite(PIN_TFT_BL, LOW);
    delay(3);
    g_level = 0;
    return;
  }
  if (g_level == 0) {
    digitalWrite(PIN_TFT_BL, HIGH);
    g_level = steps;
    delayMicroseconds(30);
  }
  int from = steps - g_level;
  int to = steps - value;
  int num = (steps + to - from) % steps;
  for (int i = 0; i < num; i++) {
    digitalWrite(PIN_TFT_BL, LOW);
    digitalWrite(PIN_TFT_BL, HIGH);
  }
  g_level = value;
}
}  // namespace display
