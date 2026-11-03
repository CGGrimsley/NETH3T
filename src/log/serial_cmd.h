#pragma once
// USB serial commands: "time <unix>", "region fcc|eu", "dump", "stats".
namespace serial_cmd {
bool rtc_init();  // reads the PCF85063; sets the system clock when it holds a valid time
void poll();
}
