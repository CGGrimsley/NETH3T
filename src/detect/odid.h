#pragma once
// OpenDroneID (ASTM F3411-22a / ASD-STAN prEN 4709-002) message decoder. Pure.
#include <cstddef>
#include <cstdint>
#include "core/observation.h"

enum class OdidErr : uint8_t { OK, TOO_SHORT, BAD_TYPE, BAD_PACK };
constexpr size_t kOdidMsgSize = 25;

// Decodes one 25-byte message or a Message Pack (type 0xF) into `out`,
// setting only the has_* groups present. `out` is not cleared first.
OdidErr odid_decode(const uint8_t* buf, size_t len, OdidFields& out);
