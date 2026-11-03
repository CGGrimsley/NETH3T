#pragma once
#include "core/target_db.h"

enum class Screen : uint8_t { TARGETS, HUNT, SPECTRUM, SETTINGS, SELFTEST };

namespace ui {
void start();  // UI task, 20 fps, core 1
void go(Screen s);
Screen current();
void show_banner(const Target& t, bool red);  // alert banner; tap opens Hunt on it
void lock_target(const Target& t);            // db lock + route radios to the target
void unlock_target();
}  // namespace ui
