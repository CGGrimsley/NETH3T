#include "core/spi_bus.h"
#include <Arduino.h>
#include <SPI.h>
#include "board_pins.h"

namespace {
SemaphoreHandle_t g_mutex = nullptr;
}

namespace spi_bus {
void init() {
  if (!g_mutex) g_mutex = xSemaphoreCreateRecursiveMutex();
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  // Keep every chip select idle-high so devices don't fight over MISO.
  for (int cs : {PIN_TFT_CS, PIN_SD_CS, PIN_LR_CS}) {
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
  }
}
Guard::Guard() { if (g_mutex) xSemaphoreTakeRecursive(g_mutex, portMAX_DELAY); }
Guard::~Guard() { if (g_mutex) xSemaphoreGiveRecursive(g_mutex); }
}  // namespace spi_bus
