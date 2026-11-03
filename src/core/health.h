#pragma once
#include <cstdint>
// Task heartbeats. A radio task that stops beating for kStallMs is treated as
// stalled: the device reboots cleanly (a task holding the shared SPI bus can't
// be killed safely) and the restart counter, kept in RTC memory, increments.
namespace health {
enum Task : uint8_t { WIFI = 0, LORA = 1, DB = 2, kCount = 3 };
constexpr uint32_t kStallMs = 15000;
void beat(Task t);
void check();             // call periodically from the UI task
uint32_t restarts();      // stall restarts since power-on
}
