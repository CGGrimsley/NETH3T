#pragma once
// BLE advertisement parser for ASTM F3411 Remote ID (service data UUID 0xFFFA,
// app code 0x0D). Pure.
#include <cstddef>
#include <cstdint>

// Walks AD structures. On match, *odid points at the ODID message (legacy: one
// 25-byte message; extended adverts: possibly a message pack). Returns false otherwise.
bool ble_find_odid(const uint8_t* adv, size_t len, const uint8_t** odid, size_t* odid_len);
