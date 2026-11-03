#pragma once
// NimBLE passive scanning for Remote ID (BLE 4 legacy + BLE 5 coded PHY).
namespace ble_scanner {
void start();
void set_continuous(bool on);  // hunt lock: scan window = interval
}
