#include "detect/ble_adv.h"

bool ble_find_odid(const uint8_t* adv, size_t len, const uint8_t** odid, size_t* odid_len) {
  if (!adv) return false;
  size_t off = 0;
  while (off < len) {
    uint8_t ad_len = adv[off];
    if (ad_len == 0) return false;  // end of significant data
    if (off + 1 + ad_len > len) return false;
    const uint8_t* d = adv + off + 1;  // d[0] = AD type
    // service data (0x16), UUID 0xFFFA (LE), app code 0x0D, counter, message(s)
    if (ad_len >= 1 + 2 + 1 + 1 + 1 && d[0] == 0x16 && d[1] == 0xFA && d[2] == 0xFF && d[3] == 0x0D) {
      *odid = d + 5;
      *odid_len = (size_t)ad_len - 5;
      return true;
    }
    off += 1 + (size_t)ad_len;
  }
  return false;
}
