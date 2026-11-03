#pragma once
#include "core/observation.h"
// CSV logger: every observation is mirrored to USB serial ("LOG," + row) and,
// when enabled and a card is present, appended to /neth3t/SNNN.csv.
namespace sd_log {
bool init();                      // mounts SD, opens the next session file, starts the log task
void write(const Observation& o); // non-blocking; drops when the queue is full
void flush();
bool active();                    // SD file open
void iso_now(char* out, size_t cap);  // "" when the clock is not set
}
