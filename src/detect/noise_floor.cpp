#include "detect/noise_floor.h"
#include <cstring>

void NoiseFloor::add_sweep(const int8_t* dbm, uint8_t n) {
  if (!dbm || n == 0) return;
  int8_t sorted[256];
  memcpy(sorted, dbm, n);
  for (uint8_t i = 1; i < n; i++) {  // insertion sort, n <= 80
    int8_t v = sorted[i];
    int j = i - 1;
    while (j >= 0 && sorted[j] > v) { sorted[j + 1] = sorted[j]; j--; }
    sorted[j + 1] = v;
  }
  p20_[head_] = sorted[(n * 2) / 10];
  head_ = (uint8_t)((head_ + 1) % kHistory);
  if (n_ < kHistory) n_++;
}

int16_t NoiseFloor::floor_dbm() const {
  if (!n_) return -127;
  int sum = 0;
  for (uint8_t i = 0; i < n_; i++) sum += p20_[i];
  return (int16_t)(sum / n_);
}

bool NoiseFloor::hot(int8_t dbm) const { return ready() && dbm > floor_dbm() + kHotDb; }
