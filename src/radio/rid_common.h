#pragma once
// Builds Remote ID observations shared by the Wi-Fi and BLE scanners.
#include <cstring>
#include <cstdio>
#include "core/observation.h"
#include "detect/id_cache.h"
#include "detect/odid.h"

// Decodes `odid` and fills `o` (type/conf/id/label). Returns false if nothing decoded.
inline bool make_rid_observation(SourceType type, const uint8_t mac[6], const uint8_t* odid,
                                 size_t odid_len, IdCache& cache, uint32_t now, Observation& o) {
  memset(&o, 0, sizeof o);
  if (odid_decode(odid, odid_len, o.odid) != OdidErr::OK) return false;
  if (!(o.odid.has_basic || o.odid.has_loc || o.odid.has_sys || o.odid.has_opid || o.odid.has_self))
    return false;
  if (o.odid.has_basic) cache.remember(mac, o.odid.uas_id, now, o.odid.id_type == 1);
  cache.resolve(mac, o.id);
  o.type = type;
  o.conf = Confidence::DECODED;
  o.t_ms = now;
  snprintf(o.label, sizeof o.label, "RID %s", o.id);
  return true;
}
