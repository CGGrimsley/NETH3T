#pragma once
// Thread-safe front end to TargetDb: radio tasks post observations into a
// queue; one task owns the table; readers take snapshots under a mutex.
#include <cstddef>
#include "core/target_db.h"

namespace db {
void start();                             // queue(64) + task on core 0
bool post(const Observation& o);          // non-blocking; false (+ drop counter) when full
size_t snapshot(Target* out, size_t max);
bool lock(SourceType t, const char* id);
void unlock();
bool locked(Target* out);                 // copies the locked target, false if none
void reset_peak();                        // on the locked target
uint32_t dropped();
void mark_alerted(SourceType t, const char* id);
bool remove(SourceType t, const char* id);
bool get(SourceType t, const char* id, Target* out);  // copy of one target
using Sink = void (*)(const Observation&);
void set_sink(Sink s);                    // called for every ingested observation (logger)
}  // namespace db
