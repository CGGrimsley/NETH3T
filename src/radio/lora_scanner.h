#pragma once
#include <cstddef>
#include <cstdint>
#include "detect/channel_plans.h"
#include "detect/rc_fingerprint.h"

// LR1121 receive-only scanner: RSSI sweeps, ELRS CAD, Meshtastic/LoRaWAN RX.
namespace lora_scanner {
bool init(int16_t* err);  // RadioLib begin + RF switch table; reads chip version
void start();             // scanner task on core 1 (no-op if init failed)
void set_region(Region r);  // plans rebuilt at the next plan boundary
void lock(uint32_t freq_khz, uint8_t sf, uint32_t bw_hz, bool fhss);
void unlock();

struct SweepRow { uint32_t f0_khz, step_khz; uint8_t n; int8_t dbm[80]; uint32_t t_ms; bool hf; int16_t floor_dbm; };
bool latest_sweep(uint8_t band /*0=915|868, 1=433, 2=2400*/, SweepRow& out);

struct Stats { uint32_t cad_hits, rx_pkts, errors, suspended; };
Stats stats();
size_t recent_cad(CadHit* out, size_t max);  // hits from the last 2 s (spectrum overlay)
const char* current_plan();                  // for the status bar
const char* version();                       // "LR1121 hw.. fw x.y" or "-"
}  // namespace lora_scanner
