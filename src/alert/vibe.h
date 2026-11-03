#pragma once
#include <cstdint>
// Vibration motor (PWM on GPIO45).
namespace vibe {
void init();
void pulse(uint16_t ms);                 // one burst
void double_pulse();                     // alarm pattern
void hunt_pulse(uint16_t pulse_hz_x10);  // 40 ms bursts at the pulse rate
void stop();
}
