#include "detect/schedule.h"

SchedStep Scheduler::next(bool focus_on) {
  visits_++;
  if (focus_on && (visits_ & 1)) return {true, 0, 0};
  SchedStep s = {false, (uint8_t)(normal_ % kSchedSlots), normal_ / kSchedSlots};
  normal_++;
  return s;
}
