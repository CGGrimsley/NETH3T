#pragma once
// Decides how loudly to announce a target. Pure.
#include <cstdint>
#include "core/target_db.h"

enum class AlertLevel : uint8_t { NONE, NOTICE /*amber, 1 short vibe*/, ALARM /*red banner, chirp, double vibe*/ };

// alert_mode: 0 off, 1 banner, 2 banner + sound + vibration. Does not modify t.
AlertLevel alert_for(const Target& t, uint8_t alert_mode);
