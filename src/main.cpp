#include <Arduino.h>
#include <Wire.h>
#include "board_pins.h"
#include "core/boot.h"
#include "core/db_task.h"
#include "core/power.h"
#include "core/spi_bus.h"
#include "radio/ble_scanner.h"
#include "radio/lora_scanner.h"
#include "radio/wifi_sniffer.h"
#include "settings/settings.h"
#include "ui/buttons.h"
#include "ui/display.h"
#include "ui/touch.h"
#include "ui/ui.h"

BootStatus g_boot = {};

namespace {
int g_boot_line = 0;

void boot_step(const char* name, bool ok) {
  Serial.printf("BOOT %s %s\n", name, ok ? "OK" : "FAIL");
  Arduino_Canvas* c = display::canvas();
  if (!c) return;
  c->setTextSize(2);
  c->setTextColor(ok ? GREEN : RED);
  c->setCursor(8, 40 + g_boot_line * 22);
  c->printf("%-8s %s", name, ok ? "OK" : "FAIL");
  g_boot_line++;
  display::flush();
}
}  // namespace

void setup() {
  pinMode(PIN_RT9080_EN, OUTPUT);
  digitalWrite(PIN_RT9080_EN, HIGH);  // power the MVSRLora back plate
  Serial.begin(115200);
  delay(200);
  Serial.println("NETH3T boot");

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  g_boot.pmu = power::init();
  spi_bus::init();
  g_boot.display = display::init();
  if (g_boot.display) {
    Arduino_Canvas* c = display::canvas();
    c->fillScreen(BLACK);
    c->setTextColor(WHITE);
    c->setTextSize(3);
    c->setCursor(8, 6);
    c->print("NETH3T");
  }
  boot_step("PMU", g_boot.pmu);
  boot_step("DISPLAY", g_boot.display);
  g_boot.touch = touch::init();
  boot_step("TOUCH", g_boot.touch);
  buttons::init();
  settings::load();
  display::set_brightness(settings::get().brightness);
  g_boot.lora = lora_scanner::init(&g_boot.lora_err);
  boot_step("LORA", g_boot.lora);
  db::start();
  lora_scanner::start();
  ui::start();
  wifi_sniffer::start();
  ble_scanner::start();
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
