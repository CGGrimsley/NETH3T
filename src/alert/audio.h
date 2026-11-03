#pragma once
#include <cstdint>
// MAX98357A speaker over I2S: alert chirp and Geiger-style hunt pulses.
namespace audio {
void init();
void chirp();                                            // 1200 Hz 80 ms + 1800 Hz 80 ms
void hunt_pulse(uint16_t tone_hz, uint16_t pulse_hz_x10); // 30 ms tone repeated at pulse rate
void stop();
void set_volume(uint8_t v);                              // 0-10
}
