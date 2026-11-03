#pragma once
// One SPI bus is shared by the display, SD card and LR1121.
// Every transaction on it must hold a Guard.
namespace spi_bus {
void init();  // creates the recursive mutex and starts SPI on pins 18/8/17
struct Guard {
  Guard();
  ~Guard();
  Guard(const Guard&) = delete;
  Guard& operator=(const Guard&) = delete;
};
}  // namespace spi_bus
