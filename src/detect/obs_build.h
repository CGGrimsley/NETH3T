#pragma once
// Frame → Observation for every receive path (Wi-Fi, BLE, LoRa packets).
// Shared by the live radios and the self-test so both run identical code. Pure.
#include <cstddef>
#include <cstdint>
#include "core/observation.h"
#include "detect/channel_plans.h"
#include "detect/id_cache.h"

struct DetectFlags { bool rid_wifi, rid_ble, drone_wifi; };

// Management frame (from Frame Control, no FCS). False if nothing to report.
bool obs_from_wifi(const uint8_t* frame, size_t len, int8_t rssi, uint8_t ch, uint32_t now,
                   const DetectFlags& f, IdCache& ids, Observation& out);
// BLE advertisement payload. `mac` MSB first. False unless it carries Remote ID.
bool obs_from_ble(const uint8_t mac[6], const uint8_t* adv, size_t len, int8_t rssi, uint32_t now,
                  const DetectFlags& f, IdCache& ids, Observation& out);
// A CRC-valid LoRa packet received on a plan of `kind`.
void obs_from_lora_packet(PlanKind kind, uint32_t freq_khz, uint8_t sf, uint32_t bw_hz, const uint8_t* buf,
                          size_t len, float rssi, float snr, uint32_t now, Observation& out);
