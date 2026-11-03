#pragma once
// CSV row formatting for SD logs and the serial mirror. Pure.
#include <cstddef>
#include "core/observation.h"

extern const char* const kCsvHeader;
// Writes one row (no trailing newline). Returns bytes written, 0 if cap is too small.
size_t csv_row(const Observation& o, const char* iso_time, char* out, size_t cap);
const char* source_type_name(SourceType t);
const char* conf_name(Confidence c);
