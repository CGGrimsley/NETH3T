#include "alert/alert_policy.h"

AlertLevel alert_for(const Target& t, uint8_t alert_mode) {
  if (alert_mode == 0 || t.alerted) return AlertLevel::NONE;
  switch (t.type) {
    case SourceType::MESHTASTIC:
    case SourceType::LORAWAN:
    case SourceType::LORA_UNKNOWN:
      return AlertLevel::NONE;  // fixed infrastructure: list only
    default:
      break;
  }
  if (t.conf == Confidence::DECODED || t.type == SourceType::ELRS) return AlertLevel::ALARM;
  return AlertLevel::NOTICE;
}
