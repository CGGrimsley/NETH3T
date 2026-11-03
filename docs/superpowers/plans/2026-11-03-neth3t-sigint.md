# NETH3T Handheld Anti-Drone & LoRa Detector — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Receive-only firmware for the LilyGO T-Display-S3-Pro + MVSRLora (LR1121) that detects Remote ID drones, ELRS/FHSS RC links, drone-maker Wi-Fi, Meshtastic and LoRaWAN, and guides the user to a locked target with an RSSI hunt screen.

**Architecture:**
- **Pure C++ modules** have no Arduino includes and are unit-tested on the host:
  parsers, classifiers, channel plans, the target table, alert policy and CSV
  formatting.
- **Hardware modules** feed `Observation`s into a FreeRTOS queue:
  - Wi-Fi promiscuous capture, NimBLE scanning, the LR1121 scanner;
  - one task owns the `TargetDb`;
  - the UI, alerts and logger read mutex-guarded snapshots.

**Tech Stack:**
- PlatformIO, `espressif32 @ 7.1.3` (Arduino core 2.x), Arduino framework.
- RadioLib 7.8.1, GFX Library for Arduino 1.4.9, SensorLib 0.5.0,
  XPowersLib 0.3.3, NimBLE-Arduino 2.5.1.
- Unity for native tests.
- This library set was compiled successfully for an ESP32-S3 target before
  the plan was written. GFX 1.6.x does **not** build on core 2.x, so do not
  upgrade it.

**Spec:** `docs/superpowers/specs/2026-11-03-neth3t-sigint-design.md`

## Global Constraints

- **Receive-only.** No code path calls `transmit()`, `startTransmit()`,
  `esp_wifi_80211_tx()` or BLE advertising. `setOutputPower` is never called.
- **Pure-module rule.** Files under `src/detect/`, plus
  `src/core/target_db.*`, `src/core/observation.h`,
  `src/alert/alert_policy.*` and `src/log/csv_format.*`, must not include
  `Arduino.h`, ESP-IDF or FreeRTOS headers. They use only `<cstdint>`,
  `<cstddef>`, `<cstring>`, `<cmath>` and `<cstdio>`.
- **Parser rule.** Every parser takes `(const uint8_t* buf, size_t len)` and
  never reads `buf[i]` for `i >= len`. It returns an error code and never
  asserts.
- **Pins.** Use exactly the values in spec §2, defined once in
  `include/board_pins.h`. Drive `PIN_RT9080_EN` (42) HIGH before touching any
  back-plate peripheral.
- **Shared SPI bus.** Display, SD and LR1121 share SPI pins 18/17/8. Every
  transaction holds `spi_bus::Guard`.
- **Builds.** `pio run -e t-display-s3-pro-lr1121` and `pio test -e native` must
  pass at the end of every task.
- **No model identifiers** in commits or code. Commits end with the
  session-supplied trailers.

## Review Focus

1. **Garbage radio frames.** Random bytes, or a frame truncated at any offset,
   in any parser must return an error and never crash.
   Owners: the `test_*_fuzz_no_crash` tests in Tasks 3, 4, 5 and 7.
2. **ODID "unknown" position.** Lat = 0 and lon = 0 is the spec's "unknown"
   encoding. Show "no fix"; never place a drone at 0,0 or compute a distance
   from it.
   Owner: `test_location_zero_is_invalid` (Task 3),
   `test_distance_requires_both_fixes` (Task 12).
3. **Crowded RF (table full).** In a city with many LoRa nodes, the table must
   keep the locked target and DECODED drones, and evict lowest confidence /
   oldest first.
   Owner: `test_eviction_prefers_low_conf_old_and_never_locked` (Task 2).
4. **Changing region while running.** FCC↔EU must rebuild every channel plan
   immediately; no stale 915 MHz plan may keep running in EU mode.
   Owner: `test_region_switch_rebuilds_plans` (Task 8).
5. **A target with one sample, or none for a while.** The trend must read
   STEADY, not WARMER/COLDER noise. The trend after a gap longer than 3 s must
   use the last real sample.
   Owner: `test_trend_single_sample_steady` and `test_trend_after_gap` (Task 2).

---

## File map

```
platformio.ini
include/board_pins.h
src/main.cpp                      bring-up order, task creation (Task 9)
src/core/observation.h            shared types (Task 2)
src/core/target_db.{h,cpp}        pure TargetDb (Task 2)
src/core/db_task.{h,cpp}          queue + mutex wrapper around TargetDb (Task 9)
src/core/spi_bus.{h,cpp}          shared-bus mutex (Task 9)
src/detect/odid.{h,cpp}           (Task 3)
src/detect/wifi_frames.{h,cpp}    (Task 4)
src/detect/ble_adv.{h,cpp}        (Task 5)
src/detect/drone_vendors.{h,cpp}  (Task 6)
src/detect/lora_headers.{h,cpp}   (Task 7)
src/detect/channel_plans.{h,cpp}  (Task 8)
src/detect/rc_fingerprint.{h,cpp} (Task 8)
src/radio/wifi_sniffer.{h,cpp}    (Task 10)
src/radio/ble_scanner.{h,cpp}     (Task 10)
src/radio/lora_scanner.{h,cpp}    (Task 11)
src/settings/settings.{h,cpp}     (Task 9)
src/ui/display.{h,cpp} touch.{h,cpp} buttons.{h,cpp}  (Task 9)
src/ui/screen_targets.cpp screen_hunt.cpp             (Task 12)
src/ui/screen_spectrum.cpp screen_settings.cpp        (Task 13)
src/ui/geo.{h,cpp}                (pure, Task 12)
src/alert/alert_policy.{h,cpp}    (pure, Task 14)
src/alert/audio.{h,cpp} vibe.{h,cpp}                  (Task 14)
src/log/csv_format.{h,cpp}        (pure, Task 15)
src/log/sd_log.{h,cpp} serial_cmd.{h,cpp}             (Task 15)
test/test_<module>/test_main.cpp  one Unity suite per pure module
test/helpers/odid_encode.h        test-only ODID encoder (Task 3)
README.md, docs/FIELD_TEST.md     (Task 16)
```

---

### Task 1: Project scaffold and both build environments

**Files:**
- Create: `platformio.ini`, `include/board_pins.h`, `src/main.cpp`, `test/test_sanity/test_main.cpp`, `.gitignore`

**Interfaces:**
- Produces:
  - env `t-display-s3-pro-lr1121` (firmware) and env `native` (host tests);
  - `board_pins.h` macros: `PIN_SPI_SCK 18`, `PIN_SPI_MOSI 17`, `PIN_SPI_MISO 8`,
    `PIN_TFT_CS 39`, `PIN_TFT_DC 9`, `PIN_TFT_RST 47`, `PIN_TFT_BL 48`,
    `TFT_W 222`, `TFT_H 480`, `TFT_COL_OFFSET 49`,
    `PIN_I2C_SDA 5`, `PIN_I2C_SCL 6`, `PIN_TOUCH_INT 21`, `PIN_TOUCH_RST 13`,
    `PIN_SD_CS 14`,
    `PIN_LR_CS 7`, `PIN_LR_IRQ 40`, `PIN_LR_RST 10`, `PIN_LR_BUSY 46`,
    `PIN_RT9080_EN 42`,
    `PIN_I2S_BCLK 4`, `PIN_I2S_LRCLK 15`, `PIN_I2S_DATA 11`, `PIN_AMP_SD 41`,
    `PIN_VIBE 45`,
    `PIN_BTN_BOOT 0`, `PIN_BTN_UP 12`, `PIN_BTN_DOWN 16`,
    `SY6970_ADDR 0x6A`.

- [ ] **Step 1: Write `platformio.ini`**
  - `[env:t-display-s3-pro-lr1121]`:
    - `platform = espressif32 @ 7.1.3`, `board = esp32-s3-devkitc-1`,
      `framework = arduino`;
    - `board_build.arduino.memory_type = qio_opi`,
      `board_upload.flash_size = 16MB`,
      `board_build.partitions = default_16MB.csv`;
    - `monitor_speed = 115200`;
    - `build_flags = -DBOARD_HAS_PSRAM -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1 -DCORE_DEBUG_LEVEL=1`;
    - `lib_deps` exactly as in the Tech Stack line, using registry names:
      `jgromes/RadioLib @ 7.8.1`,
      `moononournation/GFX Library for Arduino @ 1.4.9`,
      `lewisxhe/SensorLib @ 0.5.0`, `lewisxhe/XPowersLib @ 0.3.3`,
      `h2zero/NimBLE-Arduino @ 2.5.1`.
  - `[env:native]`:
    - `platform = native`, `test_framework = unity`, `test_build_src = yes`;
    - `build_src_filter = -<*> +<core/target_db.cpp> +<detect/> +<alert/alert_policy.cpp> +<log/csv_format.cpp> +<ui/geo.cpp>`;
    - `build_flags = -std=gnu++17 -Iinclude -Isrc -Wall -Wextra`.
- [ ] **Step 2: Write `include/board_pins.h`** with the macros listed above, and a
  comment citing the two LilyGO repos.
- [ ] **Step 3: Write a minimal `src/main.cpp`**: `setup()` drives
  `PIN_RT9080_EN` HIGH, starts Serial and prints `NETH3T boot`; `loop()` is
  empty. Write `test/test_sanity/test_main.cpp` with one test,
  `TEST_ASSERT_EQUAL(1, 1)`.
- [ ] **Step 4: Verify both envs**
  - Run `pio run -e t-display-s3-pro-lr1121`: expect `[SUCCESS]`.
  - Run `pio test -e native`: expect `1 test cases: 1 succeeded`.
  - The native filter matches no `.cpp` yet; if PlatformIO errors on the empty
    source set, add `src/detect/.keep.cpp` containing only a comment.
- [ ] **Step 5: Commit** with message
  `build: PlatformIO scaffold for T-Display-S3-Pro LR1121 + native tests`.
  `.gitignore` contains `.pio/` and `.vscode/`.

---

### Task 2: Observation types and TargetDb

**Files:**
- Create: `src/core/observation.h`, `src/core/target_db.h`, `src/core/target_db.cpp`
- Test: `test/test_target_db/test_main.cpp`

**Interfaces:**
- Produces (`observation.h`):
```c++
enum class SourceType : uint8_t { REMOTEID_WIFI, REMOTEID_BLE, DJI_LEGACY, DRONE_WIFI,
                                  ELRS, FHSS, MESHTASTIC, LORAWAN, LORA_UNKNOWN };
enum class Confidence : uint8_t { HEURISTIC = 0, FINGERPRINT = 1, DECODED = 2 };
enum class Trend : uint8_t { STEADY, WARMER, COLDER };
struct OdidFields {
  bool has_basic, has_loc, has_sys, has_opid, has_self, auth_present;
  uint8_t id_type, ua_type, status;
  char uas_id[21], operator_id[21], self_id[24];
  bool loc_valid; double lat, lon;          // loc_valid=false when unknown (0,0) or out of range
  float alt_geo_m, height_m, speed_h_ms, speed_v_ms, direction_deg;
  bool op_valid; double op_lat, op_lon; float op_alt_m;
};
struct LoraFields { uint8_t sf; uint32_t bw_hz; uint16_t rate_hz; /*0=unknown*/ };
struct Observation {
  SourceType type; Confidence conf; char id[24]; char label[24]; char vendor[16];
  uint32_t freq_khz; int16_t rssi_dbm; int8_t snr_db; uint32_t t_ms;
  OdidFields odid; LoraFields lora;
};
```
- Produces (`target_db.h`):
```c++
struct Target {  // everything in Observation, plus:
  SourceType type; Confidence conf; char id[24], label[24], vendor[16];
  uint32_t freq_khz; int16_t last_rssi, peak_rssi; int8_t snr_db; float ema_rssi;
  uint32_t first_ms, last_ms, hits; bool stale, alerted;
  struct Sample { uint32_t t_ms; float ema; } hist[64]; uint8_t hist_head, hist_len;
  OdidFields odid; LoraFields lora;
};
class TargetDb {
 public:
  static constexpr size_t kCapacity = 128;
  static constexpr uint32_t kStaleMs = 20000, kExpireMs = 60000, kTrendWindowMs = 3000;
  static constexpr float kEmaAlpha = 0.3f, kTrendDeadbandDb = 2.0f;
  void ingest(const Observation& o);
  void tick(uint32_t now_ms);
  size_t snapshot(Target* out, size_t max) const;   // sorted: conf desc, then ema_rssi desc
  Target* find(SourceType t, const char* id);
  bool lock(SourceType t, const char* id); void unlock(); const Target* locked() const;
  void reset_peak(SourceType t, const char* id);
  size_t size() const;
};
Trend trend_of(const Target& t, uint32_t now_ms);
```

- [ ] **Step 1: Write the failing tests** in `test/test_target_db/test_main.cpp`,
  using an `obs(type, id, rssi, t)` helper:
  - `test_merge_same_type_and_id`: two observations of the same key give
    `size()==1` and `hits==2`.
  - `test_same_id_different_type_separate`: `size()==2`.
  - `test_ema_smoothing`: samples −80 then −60 give `ema_rssi ≈ -74.0`
    (α 0.3; tolerance 0.01). The first sample seeds the EMA.
  - `test_peak_hold_and_reset`: peak tracks the max; `reset_peak` sets it to
    the last RSSI.
  - `test_trend_warmer_colder_steady`: EMA rising by ≥ 2 dB over 3 s is
    WARMER, falling is COLDER, ±1.5 dB is STEADY.
  - `test_trend_single_sample_steady`: one sample gives STEADY.
  - `test_trend_after_gap`: samples at t=0 (−90) and t=10000 (−60), evaluated
    at now=10000, compare against the t=0 sample and give WARMER.
  - `test_stale_then_expire`: last seen 21 s ago means `stale==true`; 61 s ago
    means removed by `tick`.
  - `test_locked_never_expires`: a locked target survives 120 s.
  - `test_eviction_prefers_low_conf_old_and_never_locked`:
    - fill 128 slots: 1 locked HEURISTIC, 10 DECODED, the rest HEURISTIC with
      ascending `last_ms`;
    - ingest a new FINGERPRINT;
    - the oldest unlocked HEURISTIC is gone, while the locked target and all
      DECODED remain.
  - `test_snapshot_order`: DECODED −90 sorts before HEURISTIC −40.
  - `test_odid_fields_merge`: a Basic ID observation then a Location
    observation for the same serial give one target with
    `has_basic && has_loc`.
  - `test_conf_upgrades_never_downgrades`: a HEURISTIC then a DECODED
    observation on the same key give DECODED.
  - `test_remoteid_wifi_and_ble_merge_by_serial`: a REMOTEID_BLE and a
    REMOTEID_WIFI observation with the same `id` give `size()==1`, with `type`
    equal to the most recent one. The merge key treats both Remote ID types as
    one class (spec §5.2).
- [ ] **Step 2: Run** `pio test -e native -f test_target_db`: expect a compile
  failure (missing header).
- [ ] **Step 3: Implement** `target_db.cpp`:
  - a fixed array of 128 slots, linear search (n is small);
  - `hist` is a ring of `{t_ms, ema}`;
  - `trend_of` compares the current EMA with the newest sample whose
    `t_ms <= now-3000`, falling back to the oldest sample;
  - ODID merge copies each `has_*` group only when the incoming flag is set;
  - the merge key maps REMOTEID_WIFI and REMOTEID_BLE to one class.
- [ ] **Step 4: Run** `pio test -e native -f test_target_db`: all PASS.
- [ ] **Step 5: Commit** `feat(core): target database with smoothing, trend, ageing`.

---

### Task 3: OpenDroneID decoder

**Files:**
- Create: `src/detect/odid.h`, `src/detect/odid.cpp`, `test/helpers/odid_encode.h`
- Test: `test/test_odid/test_main.cpp`

**Interfaces:**
- Consumes: `OdidFields` (Task 2).
- Produces:
```c++
enum class OdidErr : uint8_t { OK, TOO_SHORT, BAD_TYPE, BAD_PACK };
constexpr size_t kOdidMsgSize = 25;
OdidErr odid_decode(const uint8_t* buf, size_t len, OdidFields& out); // single msg or pack (type 0xF)
```
- Byte layout (ASTM F3411-22a; all multi-byte values little-endian):

| Byte(s) | Meaning |
|---|---|
| 0 | `(type<<4) \| proto_ver` |

  - **Basic ID (0)**: b1 = `(id_type<<4)|ua_type`; b2–21 = ID (ASCII, NUL-padded).
  - **Location (1)**:

| Byte(s) | Meaning |
|---|---|
| b1 | `(status<<4)\|(height_type<<2)\|(ew_dir<<1)\|speed_mult` |
| b2 | direction; add 180 if `ew_dir` |
| b3 | horizontal speed: ×0.25 m/s if `speed_mult==0`, else ×0.75 + 63.75 |
| b4 | vertical speed, int8 ×0.5 |
| b5–8 | lat, int32 ×1e-7 |
| b9–12 | lon, int32 ×1e-7 |
| b13–14 | pressure altitude |
| b15–16 | geodetic altitude |
| b17–18 | height |
| b19–20 | accuracies |
| b21–22 | timestamp |

    Altitudes are uint16, value×0.5 − 1000 m.
  - **Self-ID (3)**: b1 = type; b2–24 = text.
  - **System (4)**: b1 = flags; b2–5 = operator lat; b6–9 = operator lon;
    b18–19 = operator altitude (same altitude encoding).
  - **Operator ID (5)**: b1 = type; b2–21 = ID.
  - **Authentication (2)**: set `auth_present`.
  - **Message pack (0xF)**: b1 = message size (must be 25); b2 = count
    (1–9); then count × 25 bytes.

- [ ] **Step 1: Write the test-only encoder** `test/helpers/odid_encode.h`, with
  inline functions:
  `enc_basic(uint8_t* out, uint8_t id_type, uint8_t ua_type, const char* id)`,
  `enc_location(uint8_t* out, double lat, double lon, float alt_geo, float height, float speed_h, float dir)`,
  `enc_system(uint8_t* out, double op_lat, double op_lon, float op_alt)`,
  `enc_operator(uint8_t* out, const char* id)`,
  `enc_pack(uint8_t* out, const uint8_t* msgs, uint8_t count) -> size_t`.
  Protocol version 2.
- [ ] **Step 2: Write the failing tests**:
  - `test_basic_id`: decoding `enc_basic(..,1,2,"1581F5FKD229400H0Z0E")` gives
    `has_basic`, `ua_type==2` and `uas_id` equal to that string.
  - `test_location_values`:
    - input: lat 37.7749, lon −122.4194, alt_geo 120.5, height 50, speed 12.25,
      dir 270;
    - lat/lon within 1e-6, alt 120.5 ±0.5, speed 12.25 ±0.25, direction 270 ±1,
      `loc_valid`.
  - `test_location_zero_is_invalid`: lat 0, lon 0 gives `has_loc && !loc_valid`.
  - `test_location_out_of_range_invalid`: raw lat 1 000 000 000 (100°) gives
    `!loc_valid`.
  - `test_system_operator_location`: op 51.5007, −0.1246, alt 30 give
    `op_valid` and values within 1e-6 / 0.5.
  - `test_operator_id`: `"FIN87astrdge12k8"` round-trips.
  - `test_pack_with_three_messages`: a Basic, Location and System pack sets
    all three `has_*`.
  - `test_pack_bad_msg_size`: b1 = 24 gives `BAD_PACK`.
  - `test_pack_count_overflow`: count 9 but only 3 messages of data gives
    `TOO_SHORT`.
  - `test_too_short_single`: 10 bytes gives `TOO_SHORT`.
  - `test_unknown_type`: type 0x7 gives `BAD_TYPE`.
  - `test_odid_fuzz_no_crash`:
    - 10 000 iterations of an LCG (seed 1) producing random lengths 0–300 and
      random bytes;
    - every call returns without crashing;
    - also every prefix length 0..N of a valid 3-message pack.
- [ ] **Step 3: Run** `pio test -e native -f test_odid`: FAIL (missing header).
- [ ] **Step 4: Implement** `odid_decode`. Read fields through
  `rd_u16le`/`rd_i32le` helpers that take `(buf, len, off)` and fail when
  `off+n > len`.
- [ ] **Step 5: Run** `pio test -e native -f test_odid`: all PASS.
  Optionally compile with `-fsanitize=address,undefined` by adding them to the
  native `build_flags` locally (not committed) and run again: no reports.
- [ ] **Step 6: Commit** `feat(detect): OpenDroneID message decoder`.

---

### Task 4: 802.11 management frame parser (Remote ID beacon/NAN, DJI legacy, SSID)

**Files:**
- Create: `src/detect/wifi_frames.h`, `src/detect/wifi_frames.cpp`
- Test: `test/test_wifi_frames/test_main.cpp`

**Interfaces:**
- Produces:
```c++
enum class WifiKind : uint8_t { NONE, PLAIN_BEACON, ODID_BEACON, ODID_NAN, DJI_LEGACY };
struct WifiParse {
  WifiKind kind; uint8_t src[6]; char ssid[33];
  const uint8_t* odid; size_t odid_len;   // points inside the input frame
};
bool wifi_parse_mgmt(const uint8_t* frame, size_t len, WifiParse& out); // frame starts at Frame Control
extern const uint8_t kNanOdidServiceId[6]; // 88 69 19 9D 92 09 = SHA-256("org.opendroneid.remoteid")[0..5]
```
- Frame rules:
  - Frame Control byte 0: beacon `0x80`, probe response `0x50`, action `0xD0`.
  - The source address is at offset 10. Beacon and probe-response IEs start at
    24 + 12.
  - **ODID beacon**: an IE with ID `0xDD` and body
    `FA 0B BC 0D <counter> <pack…>`. `odid` points at the pack.
  - **DJI legacy**: an IE with ID `0xDD` and OUI `26 37 12`. Set the kind;
    `odid = nullptr`.
  - **NAN**:
    - action frame with addr1 `51:6F:9A:01:00:00`;
    - body: category `0x04`, action `0x09`, OUI `50 6F 9A`, type `0x13`;
    - then attributes, each `{id(1), len(2 LE), body}`;
    - a Service Descriptor attribute (ID `0x03`) whose first 6 bytes equal
      `kNanOdidServiceId` has the layout: instance(1), requestor(1),
      control(1), [service-info length(1), service info];
    - service info is `<counter> <pack…>`.
  - If only an SSID IE is present, the kind is PLAIN_BEACON.

- [ ] **Step 1: Write the failing tests**. Use builders in the test file that
  assemble frames byte by byte, and the Task 3 encoder for pack bytes.
  - `test_beacon_with_odid_ie`: kind ODID_BEACON; `odid_len==3+2*25`; `odid[0]==0xF2`;
    SSID parsed; src MAC matches.
  - `test_beacon_plain_ssid`: kind PLAIN_BEACON, SSID `"TELLO-5F3A"`.
  - `test_probe_response_treated_like_beacon`.
  - `test_nan_sdf_with_odid`: kind ODID_NAN; odid points at the pack.
  - `test_nan_wrong_service_id_ignored`: kind NONE.
  - `test_dji_legacy_ie`: kind DJI_LEGACY.
  - `test_ie_length_overruns_frame`: an IE length byte larger than the remaining
    bytes gives `false` or `kind==NONE` with no read past the end.
  - `test_non_mgmt_frame_rejected`: FC `0x08` (data) gives `false`.
  - `test_wifi_fuzz_no_crash`: same LCG pattern as Task 3, plus every truncation
    of the NAN test frame.
- [ ] **Step 2: Run** `pio test -e native -f test_wifi_frames`: FAIL.
- [ ] **Step 3: Implement** with a bounded IE iterator,
  `bool next_ie(const uint8_t* p, size_t len, size_t& off, uint8_t& id, const uint8_t*& body, uint8_t& blen)`.
- [ ] **Step 4: Run**: all PASS.
- [ ] **Step 5: Commit** `feat(detect): 802.11 beacon/NAN Remote ID frame parser`.

---

### Task 5: BLE advertisement Remote ID parser

**Files:**
- Create: `src/detect/ble_adv.h`, `src/detect/ble_adv.cpp`
- Test: `test/test_ble_adv/test_main.cpp`

**Interfaces:**
- Produces:
  `bool ble_find_odid(const uint8_t* adv, size_t len, const uint8_t** odid, size_t* odid_len);`
  - Walk the AD structures `{len, type, data}`.
  - Match type `0x16` with data `FA FF 0D <counter> <msg…>`.
  - `*odid` points at the message: 25 bytes for legacy, or a pack in extended
    adverts.

- [ ] **Step 1: Write the failing tests**:
  - `test_legacy_basic_id`: flags AD + ODID service-data AD (len `0x1E`) gives
    true and `odid_len==25`.
  - `test_extended_pack`: an AD carrying a 2-message pack gives
    `odid_len==3+50`.
  - `test_wrong_uuid` (`0xFFFB`) gives false.
  - `test_wrong_app_code` (`0x0C`) gives false.
  - `test_zero_length_ad_terminates`: a zero-length AD ends the walk without
    looping.
  - `test_ble_fuzz_no_crash`.
- [ ] **Step 2: Run** `pio test -e native -f test_ble_adv`: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run**: PASS.
- [ ] **Step 5: Commit** `feat(detect): BLE Remote ID advert parser`.

---

### Task 6: Drone-maker OUI and SSID tables

**Files:**
- Create: `src/detect/drone_vendors.h`, `src/detect/drone_vendors.cpp`
- Test: `test/test_drone_vendors/test_main.cpp`

**Interfaces:**
- Produces:
  `const char* drone_vendor_by_oui(const uint8_t mac[6]);`
  `const char* drone_vendor_by_ssid(const char* ssid);`
  Both return the vendor name, or `nullptr`.
- **OUI table** (verified against IEEE `oui.csv` on 2026-11-03; cite it in a
  comment):
  - **DJI** ("SZ DJI TECHNOLOGY CO." and "DJI BAIWANG TECHNOLOGY"):
    `60:60:1F 48:1C:B9 8C:58:23 0C:9A:E6 E4:7A:2C 88:29:85 58:B8:58 34:D2:62
    04:A8:5A 4C:43:F6 9C:5A:8A C8:A1:62 EC:72:F7 34:91:F0`.
  - **Parrot**: `90:3A:E6 00:12:1C 90:03:B7 A0:14:3D 00:26:7E`.
  - **Skydio**: `38:1D:14`.
  - Excluded: DJI Ronin and DJI Osmo (gimbals and cameras, not aircraft);
    "Beijing Autelan" (unrelated to Autel Robotics).
- **SSID rules**, case-insensitive:

| Match | Vendor |
|---|---|
| prefix `TELLO-` | Ryze/DJI Tello |
| prefix `DJI-` | DJI |
| contains `MAVIC` | DJI |
| prefix `SPARK-` | DJI |
| contains `PHANTOM` | DJI |
| contains `ANAFI` | Parrot |
| prefix `BEBOP` | Parrot |
| prefix `DISCO-` | Parrot |
| prefix `AUTEL` | Autel |
| prefix `EVO` followed by `_`, `-` or `II` | Autel |
| prefix `SKYDIO` | Skydio |
| prefix `YUNEEC` | Yuneec |
| prefix `HUBSAN` | Hubsan |
| prefix `HOLYSTONE` / `HS-` | Holy Stone |

- [ ] **Step 1: Write the failing tests**:
  - `test_dji_oui` (`60:60:1F:..`) gives `"DJI"`.
  - `test_parrot_oui` (`90:03:B7:..`) gives `"Parrot"`.
  - `test_skydio_oui` gives `"Skydio"`.
  - `test_router_oui_no_match`: TP-Link `50:C7:BF:..` gives nullptr.
  - `test_ssid_tello` (`"tello-5F3A"`) gives `"Ryze/DJI Tello"`.
  - `test_ssid_anafi` (`"Anafi-123456"`) gives `"Parrot"`.
  - `test_ssid_no_false_positive`: `"Evolution_Home"`, `"HomeWiFi"`,
    `"DIRECT-xy"` and `"Everest"` all give nullptr.
  - `test_ssid_empty_or_null` gives nullptr.
- [ ] **Step 2: Run**: FAIL.
- [ ] **Step 3: Implement.** Store OUIs as a sorted `uint32_t` array and use
  binary search.
- [ ] **Step 4: Run**: PASS.
- [ ] **Step 5: Commit** `feat(detect): drone vendor OUI/SSID tables`.

---

### Task 7: Meshtastic and LoRaWAN header parsers

**Files:**
- Create: `src/detect/lora_headers.h`, `src/detect/lora_headers.cpp`
- Test: `test/test_lora_headers/test_main.cpp`

**Interfaces:**
- Produces:
```c++
struct MeshHeader { uint32_t dest, sender, packet_id; uint8_t hop_limit, hop_start, channel_hash; bool want_ack; };
bool parse_meshtastic(const uint8_t* p, size_t len, MeshHeader& out);  // needs len >= 16
enum class LwType : uint8_t { JOIN_REQUEST, JOIN_ACCEPT, UNCONF_UP, UNCONF_DOWN, CONF_UP, CONF_DOWN, REJOIN, PROPRIETARY };
struct LwHeader { LwType type; uint32_t dev_addr; uint16_t fcnt; uint64_t join_eui, dev_eui; };
bool parse_lorawan(const uint8_t* p, size_t len, LwHeader& out);
void format_mesh_id(uint32_t sender, char out[24]);        // "!%08x"
void format_lw_id(const LwHeader& h, char out[24]);       // "DA:%08X" or "EUI:%016llX"
```
- **Meshtastic** (little-endian):

| Byte(s) | Field |
|---|---|
| 0–3 | dest |
| 4–7 | sender |
| 8–11 | packet id |
| 12 | flags: hop_limit = bits 0–2, want_ack = bit 3, hop_start = bits 5–7 |
| 13 | channel hash |

- **LoRaWAN**: MType = MHDR bits 7–5.
  - Data frames: DevAddr = bytes 1–4 LE, FCnt = bytes 6–7 LE; minimum 12 bytes.
  - Join-request: JoinEUI = bytes 1–8 LE, DevEUI = bytes 9–16 LE; length must
    be exactly 23.

- [ ] **Step 1: Write the failing tests**:
  - `test_mesh_header_fields`: sender `0xA1B2C3D4`, flags `0x63` give
    hop_limit 3, hop_start 3, want_ack 0, and id `"!a1b2c3d4"`.
  - `test_mesh_too_short` (15 bytes) gives false.
  - `test_lw_unconf_up`: `40 04 03 02 01 80 2A 00 …` give DevAddr
    `0x01020304`, FCnt 42, id `"DA:01020304"`.
  - `test_lw_join_request`: 23 bytes give the right EUIs.
  - `test_lw_join_request_wrong_len` gives false.
  - `test_lw_too_short` gives false.
  - `test_lora_fuzz_no_crash`.
- [ ] **Step 2: Run**: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run**: PASS.
- [ ] **Step 5: Commit** `feat(detect): Meshtastic and LoRaWAN header parsers`.

---

### Task 8: Channel plans and RC link fingerprinting

**Files:**
- Create: `src/detect/channel_plans.{h,cpp}`, `src/detect/rc_fingerprint.{h,cpp}`
- Test: `test/test_channel_plans/test_main.cpp`, `test/test_rc_fingerprint/test_main.cpp`

**Interfaces:**
- Produces (`channel_plans.h`):
```c++
enum class Region : uint8_t { FCC, EU };
enum class PlanMode : uint8_t { RSSI_SWEEP, CAD, RX };
struct ScanPlan {
  const char* name; PlanMode mode; uint32_t freqs_khz[80]; uint8_t n_freqs;
  uint8_t sfs[4]; uint8_t n_sfs; uint32_t bw_hz; uint8_t sync_word; uint16_t rx_timeout_ms;
  bool hf; // 2.4 GHz
};
size_t build_plans(Region r, ScanPlan* out, size_t max);   // returns count
uint32_t meshtastic_freq_khz(Region r, const char* preset_name, uint32_t bw_hz);
uint16_t elrs_rate_hz(bool band24, uint8_t sf);            // 0 if unknown
```

  Plans built for each region:
  - **RSSI sweeps**, 200 kHz steps, at most 80 points (stride widened to fit):
    - FCC: 902–928 MHz;
    - EU: 863–870 MHz;
    - both: 433.05–434.79 MHz, and 2400–2480 MHz (1 MHz steps, `hf=true`).
  - **ELRS 900 CAD**, BW 500 000, SFs {6, 7, 8, 9}:
    - FCC: 40 channels, 903 500 + i×600 kHz;
    - EU: 13 channels, 863 275 + i×525 kHz.
  - **ELRS 2.4 CAD**, BW 812 500, SFs {5, 6, 7, 8}, `hf=true`: 80 channels,
    2 400 400 + i×1 000 kHz.
  - **Meshtastic RX**, sync word 0x2B, timeout 1500 ms, one plan per preset:
    - LongFast: SF11, BW 250 k;
    - MediumFast: SF9, BW 250 k;
    - ShortFast: SF7, BW 250 k;
    - LongSlow: SF12, BW 125 k.
  - **LoRaWAN RX**, sync word 0x34, SFs {7, 8, 9, 10}, BW 125 k, timeout 1000 ms:
    - FCC: 903 900 + i×200 kHz for i = 0..7;
    - EU: 868 100, 868 300, 868 500.

  Meshtastic frequency:
  - FCC band 902–928 MHz; EU band 869.4–869.65 MHz.
  - `n = floor(span / bw)`, `slot = djb2(name) % n`,
    `f = start + bw/2 + slot × bw`.
  - djb2: `h = 5381; h = h*33 + c` over the preset name (uint32).

  ELRS packet rate from SF:
  - 900 MHz: SF6 → 200, SF7 → 100, SF8 → 50, SF9 → 25 Hz.
  - 2.4 GHz: SF5 → 500, SF6 → 250, SF7 → 150, SF8 → 50 Hz.
- Produces (`rc_fingerprint.h`):
```c++
struct CadHit { uint32_t freq_khz; uint8_t sf; uint32_t bw_hz; uint32_t t_ms; int16_t rssi; };
struct HotBin { uint32_t freq_khz; uint32_t t_ms; int16_t rssi; };
class RcFingerprint {
 public:
  static constexpr uint32_t kWindowMs = 2000;
  void add_cad(const CadHit& h);
  void add_hot(const HotBin& b);
  // Emits zero or more Observations (ELRS/FHSS, synthetic ids "ELRS900-SF7", "ELRS24-SF6", "FHSS915", "FHSS868")
  size_t evaluate(uint32_t now_ms, Observation* out, size_t max);
};
```
  Classification rules:
  - **ELRS**: within the window, ≥ 4 CAD hits at the same `(bw, sf)` on ≥ 3
    distinct frequencies. Confidence FINGERPRINT; label `ELRS 915 ~100Hz`.
  - **FHSS**: within the window, hot bins at ≥ 6 distinct frequencies in the
    902–928 or 863–870 range, where no single frequency appears in more than
    40 % of the bins, and no ELRS was classified for that band in the same
    window. Confidence HEURISTIC.

- [ ] **Step 1: Write the failing tests** in `test_channel_plans`:
  - `test_meshtastic_longfast_us_906875`: `meshtastic_freq_khz(FCC,"LongFast",250000)==906875`.
  - `test_meshtastic_mediumfast_us_913125`.
  - `test_meshtastic_eu_single_slot_869525`.
  - `test_elrs900_fcc_40ch`: first 903 500, last 926 900, count 40.
  - `test_elrs900_eu_13ch`: first 863 275, last 869 575.
  - `test_elrs24_80ch`: first 2 400 400, last 2 479 400.
  - `test_elrs_rate_map`: values as listed; `elrs_rate_hz(false, 12)==0`.
  - `test_region_switch_rebuilds_plans`: for every EU plan, no frequency lies
    in 902–928 MHz; for every FCC plan, none lies in 863–870 MHz.
  - `test_sweep_respects_80_point_cap`.
- [ ] **Step 2: Write the failing tests** in `test_rc_fingerprint`:
  - `test_elrs900_detected`: 6 SF7/BW500 hits on 4 channels within 1.5 s give
    one Observation with type ELRS, id `"ELRS900-SF7"`, `rate_hz` 100 and
    confidence FINGERPRINT.
  - `test_elrs24_detected`: SF6/BW812.5 hits give `"ELRS24-SF6"` at 250 Hz.
  - `test_too_few_channels_no_elrs`: 6 hits on 2 channels give nothing.
  - `test_hits_outside_window_ignored`: hits spread over 5 s give nothing.
  - `test_fhss_detected`: 8 hot bins on 8 frequencies in 915 give `"FHSS915"`,
    HEURISTIC.
  - `test_static_carrier_not_fhss`: 10 bins, 9 of them on one frequency, give
    nothing.
  - `test_elrs_suppresses_fhss_same_band`.
  - `test_rssi_is_max_of_window`.
- [ ] **Step 3: Run** `pio test -e native -f test_channel_plans -f test_rc_fingerprint`:
  FAIL.
- [ ] **Step 4: Implement.** `RcFingerprint` keeps ring buffers of 128 hits and
  128 bins and prunes anything older than the window on each call.
- [ ] **Step 5: Run**: PASS.
- [ ] **Step 6: Commit** `feat(detect): region channel plans and ELRS/FHSS fingerprinting`.

---

### Task 9: Board bring-up — SPI bus, display, touch, buttons, settings, db task

**Files:**
- Create: `src/core/spi_bus.{h,cpp}`, `src/core/db_task.{h,cpp}`, `src/core/power.{h,cpp}`, `src/settings/settings.{h,cpp}`, `src/ui/display.{h,cpp}`, `src/ui/touch.{h,cpp}`, `src/ui/buttons.{h,cpp}`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `TargetDb`, `Observation` (Task 2); `Region` (Task 8).
- Produces:
```c++
namespace spi_bus { void init(); struct Guard { Guard(); ~Guard(); }; }   // recursive mutex, SPI.begin(18,8,17)
namespace db { void start();                         // creates queue(64) + task on core 0
               bool post(const Observation& o);      // non-blocking; false + drop counter on full
               size_t snapshot(Target* out, size_t max);
               bool lock(SourceType, const char*); void unlock(); bool locked(Target* out);
               void reset_peak(); uint32_t dropped();
               void mark_alerted(SourceType, const char* id); }
namespace power { bool init(); int8_t battery_pct(); /*-1 unknown*/ bool charging(); }  // src/core/power.{h,cpp}, XPowersLib PowersSY6970
struct Settings { Region region; bool en_rid_wifi, en_rid_ble, en_drone_wifi, en_elrs, en_fhss,
                  en_mesh, en_lorawan, show_lora_nodes, sd_log, sound, vibe;
                  uint8_t alert_mode /*0 off,1 banner,2 full*/, volume /*0-10*/, brightness /*1-16*/; };
namespace settings { void load(); void save(); Settings& get(); }  // Preferences namespace "neth3t"
namespace display { bool init(); Arduino_Canvas* canvas(); void flush(); void set_brightness(uint8_t); }
namespace touch { bool init(); bool read(int16_t& x, int16_t& y); }
enum class Btn : uint8_t { NONE, BACK, UP, DOWN, UP_LONG, DOWN_LONG };
namespace buttons { void init(); Btn poll(); }     // 30 ms debounce, 700 ms long press
struct BootStatus { bool pmu, display, touch, lora, sd; int16_t lora_err; };
extern BootStatus g_boot;
```
- Bring-up order in `setup()` (spec §7):
  1. RT9080 EN.
  2. `Wire.begin(5,6)`, then PMU (`PowersSY6970`).
  3. `spi_bus::init`, then `display::init` (`Arduino_ST7796` with col offset
     49, rotation 0; 222×480 canvas in PSRAM).
  4. Touch (`TouchDrvCSTXXX`, CST226SE, RST 13, INT 21).
  5. `settings::load`.
  6. `db::start`.

  LR1121, SD and the radios are added in Tasks 10, 11 and 15.

  Print each step as `BOOT <name> OK|FAIL` to serial and to a boot-screen line.
- `display::flush()` pushes the canvas in 40-row strips, taking
  `spi_bus::Guard` for each strip, so a strip holds the bus for under 10 ms.
- Backlight: LEDC on `PIN_TFT_BL`, 16 levels. Board V1.1 uses the 16-level
  driver.

- [ ] **Step 1: Implement** the modules above.
- [ ] **Step 2: Build** `pio run -e t-display-s3-pro-lr1121`: expect `[SUCCESS]`.
  Then `pio test -e native`: all earlier suites still PASS.
- [ ] **Step 3: Add the device check** to `docs/FIELD_TEST.md`, section
  "Bring-up": the boot screen lists PMU, DISPLAY and TOUCH as OK; a touch
  draws a dot; each button prints its `Btn` value over serial.
- [ ] **Step 4: Commit** `feat(board): SPI bus, display, touch, buttons, settings, target db task`.

---

### Task 10: 2.4 GHz detectors — Wi-Fi sniffer and BLE scanner

**Files:**
- Create: `src/radio/wifi_sniffer.{h,cpp}`, `src/radio/ble_scanner.{h,cpp}`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes:
  - `wifi_parse_mgmt` (Task 4), `odid_decode` (Task 3), `ble_find_odid` (Task 5);
  - `drone_vendor_by_oui`/`_ssid` (Task 6);
  - `db::post` (Task 9), `settings::get()`.
- Produces:
```c++
namespace wifi_sniffer { void start(); void pin_channel(uint8_t ch /*0 = resume hopping*/); uint8_t channel(); }
namespace ble_scanner { void start(); void set_continuous(bool); }
```
- **Wi-Fi**:
  - `WIFI_MODE_NULL` + promiscuous mode, filter `WIFI_PROMIS_FILTER_MASK_MGMT`.
  - The callback copies at most 512 bytes into a 16-deep queue; parsing happens
    in a task, never in the callback.
  - Hop order: 1, 6, 11, 2, 7, 12, 3, 8, 13, 4, 9, 5, 10.
    Dwell 120 ms, 250 ms on channel 6.
  - Observation rules:

| Parse result | Observation |
|---|---|
| ODID_BEACON / ODID_NAN | decode; type REMOTEID_WIFI; DECODED; id = `uas_id` if `has_basic`, else MAC `XX:XX:XX:XX:XX:XX`; `freq_khz = 2407000 + ch×5000` |
| DJI_LEGACY | DJI_LEGACY, FINGERPRINT, id = MAC |
| PLAIN_BEACON with a vendor match | DRONE_WIFI, HEURISTIC, label = SSID, vendor set |

- **BLE**:
  - NimBLE scan with `setExtendedScan`-style coded and 1M PHY on alternate
    windows (NimBLE 2.x API: `NimBLEScan::setPhy`), window 300 ms per 1000 ms,
    `setDuplicateFilter(false)`.
  - Payloads go through `ble_find_odid`, then `odid_decode`.
  - Type REMOTEID_BLE, DECODED, id = `uas_id` or MAC.
  - `freq_khz = 2402000` (advert channel unknown).
- Respect `settings::get().en_*`.
- **Hunt lock**: `pin_channel(ch)` for Wi-Fi targets, `set_continuous(true)`
  for BLE targets.

- [ ] **Step 1: Implement** both modules and start them from `setup()` after
  `db::start`.
- [ ] **Step 2: Build**: `[SUCCESS]`. Native tests: PASS.
- [ ] **Step 3: Add to FIELD_TEST.md**, section "Remote ID":
  - An Android phone running the OpenDroneID "Transmitter" app (BLE legacy
    mode) appears in `dump` over serial as `REMOTEID_BLE` with the app's serial
    and location.
  - Switching the app to Wi-Fi Beacon updates the same target (same serial),
    now reported as `REMOTEID_WIFI`.
- [ ] **Step 4: Commit** `feat(radio): Wi-Fi promiscuous and BLE Remote ID scanners`.

---

### Task 11: LR1121 scanner — sweeps, CAD, RX, hunt lock

**Files:**
- Create: `src/radio/lora_scanner.{h,cpp}`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes:
  - `build_plans`, `ScanPlan` (Task 8); `RcFingerprint` (Task 8);
  - `parse_meshtastic`, `parse_lorawan`, `format_*_id` (Task 7);
  - `spi_bus::Guard`, `db::post`, `settings::get()`.
- Produces:
```c++
namespace lora_scanner {
  bool init(int16_t* err);                    // RadioLib LR1121 begin, RF switch table, version read
  void start();                               // task on core 1
  void set_region(Region r);                  // rebuilds plans at next plan boundary
  void lock(uint32_t freq_khz, uint8_t sf, uint32_t bw_hz, bool fhss); void unlock();
  struct SweepRow { uint32_t f0_khz, step_khz; uint8_t n; int8_t dbm[80]; uint32_t t_ms; bool hf; };
  bool latest_sweep(uint8_t band /*0=915|868,1=433,2=2400*/, SweepRow& out);
  struct Stats { uint32_t cad_hits, rx_pkts, errors, suspended; };
  Stats stats();
  size_t recent_cad(CadHit* out, size_t max);  // hits from the last 2 s, for the spectrum overlay
}
```
- **RadioLib setup**:
  - `LR1121 radio = new Module(PIN_LR_CS, PIN_LR_IRQ, PIN_LR_RST, PIN_LR_BUSY, SPI)`.
  - RF switch DIO5/DIO6 table exactly as in spec §2. Set it before `begin()`.
  - `begin()` with the default LoRa settings.
- **Modes**:
  - **RSSI sweep**: `setFrequency(f)`, `startReceive()`, wait 1 ms, then
    `getRSSI(false, true)`.
    - Noise floor per band = running 20th percentile of the last 16 sweeps.
    - Bins more than 10 dB above the floor go to `add_hot`.
  - **CAD**: `setBandwidth(bw, hf)`, `setSpreadingFactor(sf)`, then
    `scanChannel()`.
    - `RADIOLIB_LORA_DETECTED` means a hit: `add_cad` with RSSI from
      `getRSSI(false, true)`.
  - **RX**: `setSyncWord`, explicit header, `startReceive()`, wait for IRQ or
    timeout, then `readData`.
    - Parse with the Meshtastic or LoRaWAN parser for the plan.
    - Packet RSSI/SNR via `getRSSI()`/`getSNR()`.
    - A parse failure on a valid CRC gives LORA_UNKNOWN (HEURISTIC, id
      `"LORA-<freqMHz>-SF<n>"`).
- **Error handling**: wrap every SPI-using call in `spi_bus::Guard`. Five
  consecutive non-zero RadioLib codes in a plan suspend that plan for 30 s
  (`suspended++`).
- **Scheduling**:
  - Interleave: sweep(915/868), ELRS900, Meshtastic preset (rotating),
    sweep(2400), ELRS24, LoRaWAN, sweep(433), and repeat.
  - Call `RcFingerprint::evaluate` after each plan and post its results.
- **Hunt lock**:
  - CAD targets: run CAD only on the locked frequency set and SF.
  - RX targets: stay on that RX plan.
  - FHSS: sweep only the locked band.
- If `init` fails, set `g_boot.lora=false` and `g_boot.lora_err`; `start()`
  becomes a no-op.

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Build**: `[SUCCESS]`. Native: PASS.
  Then `grep -rn "transmit\|setOutputPower" src/`: expect no matches
  (receive-only constraint).
- [ ] **Step 3: Add to FIELD_TEST.md**, section "LoRa / RC":
  - an ELRS TX at 10 mW in 915 or 868 mode, then 2.4 mode, shows `ELRS900-SFx`
    or `ELRS24-SFx` in `dump`;
  - a Meshtastic node on LongFast shows `!xxxxxxxx`;
  - `stats` over serial shows the CAD/RX counters rising.
- [ ] **Step 4: Commit** `feat(radio): LR1121 sweep/CAD/RX scanner with hunt lock`.

---

### Task 12: Targets screen and Hunt screen

**Files:**
- Create: `src/ui/ui.{h,cpp}` (screen router + status bar), `src/ui/screen_targets.cpp`, `src/ui/screen_hunt.cpp`, `src/ui/geo.{h,cpp}`
- Test: `test/test_geo/test_main.cpp`
- Modify: `src/main.cpp` (UI task at 20 fps on core 1)

**Interfaces:**
- Consumes:
  - `db::snapshot/lock/unlock/locked/reset_peak`, `trend_of`;
  - `display::*`, `touch::read`, `buttons::poll`;
  - `wifi_sniffer::pin_channel`, `ble_scanner::set_continuous`,
    `lora_scanner::lock/unlock`;
  - `g_boot`, `power::battery_pct()` and `power::charging()` (Task 9).
- Produces:
```c++
enum class Screen : uint8_t { TARGETS, HUNT, SPECTRUM, SETTINGS };
namespace ui { void start(); void go(Screen s); Screen current(); void show_banner(const Target& t, bool red); }
// geo.h (pure)
double haversine_m(double lat1, double lon1, double lat2, double lon2);
bool drone_pilot_distance_m(const OdidFields& f, double* out_m);   // false unless loc_valid && op_valid
uint16_t rssi_to_pulse_hz_x10(float rssi);   // -110 dBm → 10 (1.0 Hz), -40 → 100 (10 Hz), clamped, linear
uint16_t rssi_to_tone_hz(float rssi);        // -110 → 400, -40 → 2000, clamped, linear
```
- **Status bar** (24 px): region, LoRa plan name, `SD ✓/–`, `LORA ✕ <code>`
  when it failed, battery %.
- **Targets**: 44 px rows.
  - Colors: DECODED or ELRS red `#E5484D`, HEURISTIC amber `#F5A524`, LoRa
    nodes grey `#8B8D98`, stale rows at 50 % brightness.
  - Hide LoRa nodes when `show_lora_nodes` is false.
  - Up/down moves the selection, a tap or `UP_LONG` locks and opens Hunt,
    `DOWN_LONG` opens Settings, a swipe left opens Spectrum.
- **Hunt**, as in spec §6:
  - meter range −120 to −30 dBm;
  - trend arrow from `trend_of`;
  - peak line with a "reset" touch zone;
  - 30 s history graph from `hist`;
  - ODID panel with "no fix" whenever `!loc_valid` / `!op_valid`;
  - hint line copied verbatim from spec §6.
- `BACK` unlocks and returns to Targets.
- **Locking routes the radios** by the target's type:
  - Wi-Fi → `pin_channel`;
  - BLE → `set_continuous`;
  - ELRS/FHSS/LoRa → `lora_scanner::lock`.

- [ ] **Step 1: Write the failing tests** in `test/test_geo`:
  - `test_haversine_known`: (51.5007, −0.1246) → (40.6892, −74.0445) ≈
    5 574 840 m ±0.5 %.
  - `test_distance_requires_both_fixes`: `op_valid=false` gives false.
  - `test_pulse_mapping`: −110 gives 10, −40 gives 100, −75 gives 55, −200 is
    clamped to 10, 0 is clamped to 100.
  - `test_tone_mapping`: −110 gives 400, −40 gives 2000.
- [ ] **Step 2: Run** `pio test -e native -f test_geo`: FAIL. Implement
  `geo.cpp`. Run again: PASS.
- [ ] **Step 3: Implement** the UI modules.
- [ ] **Step 4: Build**: `[SUCCESS]`.
- [ ] **Step 5: Add to FIELD_TEST.md**, section "Hunt":
  - lock onto the phone's Remote ID;
  - walking away makes the arrow go COLDER within about 3 s, walking back makes
    it WARMER;
  - peak holds;
  - the ODID panel shows the app's coordinates.
- [ ] **Step 6: Commit** `feat(ui): targets list and hunt screen`.

---

### Task 13: Spectrum screen and Settings screen

**Files:**
- Create: `src/ui/screen_spectrum.cpp`, `src/ui/screen_settings.cpp`
- Modify: `src/ui/ui.cpp`

**Interfaces:**
- Consumes:
  - `lora_scanner::latest_sweep`, `lora_scanner::recent_cad`,
    `lora_scanner::set_region`, `lora_scanner::stats`;
  - `settings::get/save`, `display::set_brightness`, `db::dropped`.
- **Spectrum**:
  - Up/down cycles band 0/1/2.
  - Waterfall of 200 rows × `n` columns; color ramp from noise floor to
    floor + 40 dB.
  - The latest CAD hits are drawn as tick marks labelled `SFn`.
- **Settings**: a list of every `Settings` field.
  - Tapping toggles or cycles the value and calls `settings::save()` immediately.
  - Changing the region calls `lora_scanner::set_region`.
  - **About/debug** rows: LR1121 version (from `init`), `stats()` counters,
    `db::dropped()`, task restarts, `ESP.getFreeHeap()`.

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Build**: `[SUCCESS]`. Native: PASS.
- [ ] **Step 3: Add to FIELD_TEST.md**: switching the region to EU and back
  makes the spectrum's band 0 axis change between 863–870 and 902–928 MHz;
  settings survive a reboot.
- [ ] **Step 4: Commit** `feat(ui): spectrum waterfall and settings screen`.

---

### Task 14: Alerts — policy, speaker tones, vibration

**Files:**
- Create: `src/alert/alert_policy.{h,cpp}` (pure), `src/alert/audio.{h,cpp}`, `src/alert/vibe.{h,cpp}`
- Test: `test/test_alert_policy/test_main.cpp`
- Modify: `src/ui/ui.cpp`, `src/main.cpp`

**Interfaces:**
- Consumes: `Target`, `rssi_to_pulse_hz_x10`, `rssi_to_tone_hz` (Task 12),
  `settings::get()`.
- Produces:
```c++
enum class AlertLevel : uint8_t { NONE, NOTICE /*amber, 1 short vibe*/, ALARM /*red banner, chirp, double vibe*/ };
AlertLevel alert_for(const Target& t, uint8_t alert_mode);   // marks nothing; caller sets t.alerted
// rules: alert_mode 0 → NONE; t.alerted → NONE; MESHTASTIC/LORAWAN/LORA_UNKNOWN → NONE;
//        conf DECODED or type ELRS → ALARM; HEURISTIC/FINGERPRINT otherwise → NOTICE
namespace audio { void init(); void chirp(); void hunt_pulse(uint16_t tone_hz, uint16_t pulse_hz_x10); void stop(); void set_volume(uint8_t); }
namespace vibe  { void init(); void pulse(uint16_t ms); void hunt_pulse(uint16_t pulse_hz_x10); void stop(); }
```
- **Audio**:
  - I²S on 4/15/11 with `PIN_AMP_SD` (41) HIGH to enable the amp; 16 kHz mono
    square wave.
  - Chirp = 1 200 Hz for 80 ms, then 1 800 Hz for 80 ms.
  - Hunt pulse = 30 ms tone at `tone_hz`, repeated at `pulse_hz_x10/10` Hz.
- **Vibration**: LEDC PWM on 45, 40 ms bursts.
- **Behavior**:
  - The alert loop in the UI task runs once per second over new snapshot
    entries and sets `alerted` through
    `db::mark_alerted(type, id)` (Task 9).
  - An aged-out target that returns is a fresh `Target`, so it can alert again.
  - Hunt audio and vibration run only on the Hunt screen, gated by
    `settings.sound` / `settings.vibe`.

- [ ] **Step 1: Write the failing tests**:
  - `test_mode_off_none`.
  - `test_already_alerted_none`.
  - `test_decoded_drone_alarm` (REMOTEID_BLE, DECODED).
  - `test_elrs_alarm` (FINGERPRINT ELRS).
  - `test_heuristic_notice` (DRONE_WIFI).
  - `test_dji_legacy_notice`.
  - `test_lora_nodes_never_alert` (MESHTASTIC DECODED gives NONE).
- [ ] **Step 2: Run**: FAIL. Implement `alert_policy.cpp`. Run: PASS.
- [ ] **Step 3: Implement** audio, vibe and the UI wiring (banner tap opens
  Hunt for that target).
- [ ] **Step 4: Build**: `[SUCCESS]`.
- [ ] **Step 5: Add to FIELD_TEST.md**: starting the phone transmitter gives one
  red banner, a chirp and a double vibration; no repeat while it stays visible.
- [ ] **Step 6: Commit** `feat(alert): alert policy, speaker chirp and hunt tones, vibration`.

---

### Task 15: SD CSV logging, serial mirror, serial commands, RTC time

**Files:**
- Create: `src/log/csv_format.{h,cpp}` (pure), `src/log/sd_log.{h,cpp}`, `src/log/serial_cmd.{h,cpp}`
- Test: `test/test_csv_format/test_main.cpp`
- Modify: `src/main.cpp`, `src/core/db_task.cpp` (forward every ingested Observation to `sd_log::write`)

**Interfaces:**
- Consumes: `Observation`; `db::snapshot`; `settings`; SensorLib `SensorPCF85063`.
- Produces:
```c++
extern const char* const kCsvHeader;  // "t_ms,iso_time,type,conf,id,freq_khz,rssi,snr,sf,bw_khz,lat,lon,alt_m,op_lat,op_lon,vendor,extra"
size_t csv_row(const Observation& o, const char* iso_time, char* out, size_t cap); // returns bytes, 0 if cap too small
const char* source_type_name(SourceType);  // "REMOTEID_WIFI", ...
const char* conf_name(Confidence);         // "DECODED", "FINGERPRINT", "HEURISTIC"
namespace sd_log { bool init(); void write(const Observation& o); void flush(); bool active(); }
namespace serial_cmd { void poll(); }   // "time <unix>", "region fcc|eu", "dump", "stats"
```
- **CSV rules**:
  - Fields containing `,` `"` or newline are quoted, with `"` doubled.
  - Lat/lon are printed with 7 decimals only when `loc_valid`/`op_valid`;
    otherwise empty.
  - `bw_khz` has 1 decimal.
  - `iso_time` is empty when the RTC is unset.
- **SD**:
  - `SD.begin(PIN_SD_CS, SPI)` under the guard.
  - Directory `/neth3t`, file `/neth3t/S%03u.csv` = the first unused number.
  - 4 KB PSRAM buffer, flushed every 2 s or when full.
  - When `settings.sd_log` is false or SD is missing, `write` only mirrors to
    serial.
- **Serial mirror**: `LOG,` + row.
- **RTC**: `time <unix>` sets the PCF85063. At boot the RTC counts as valid when
  its year is ≥ 2025.

- [ ] **Step 1: Write the failing tests**:
  - `test_header_exact`.
  - `test_row_remoteid_full`: lat 37.7749 prints `37.7749000`.
  - `test_row_invalid_loc_empty_fields`.
  - `test_row_quotes_comma_in_ssid` (label `a,b` gives `"a,b"`).
  - `test_row_truncated_buffer_returns_0`.
  - `test_type_names_cover_all_enums`.
- [ ] **Step 2: Run**: FAIL. Implement `csv_format.cpp`. Run: PASS.
- [ ] **Step 3: Implement** sd_log, serial_cmd and the RTC hookup; add
  `BOOT SD` and `BOOT RTC` lines.
- [ ] **Step 4: Build**: `[SUCCESS]`. Native: all PASS.
- [ ] **Step 5: Add to FIELD_TEST.md**: after a 5-minute session, the SD card
  has `/neth3t/S000.csv` with a header and rows; `time $(date +%s)` sets the
  clock and later rows have `iso_time`.
- [ ] **Step 6: Commit** `feat(log): SD CSV logger, serial mirror and commands`.

---

### Task 16: README overhaul, field-test doc, final verification

**Files:**
- Modify: `README.md` (full rewrite), `docs/FIELD_TEST.md` (finalize)

- [ ] **Step 1: Rewrite `README.md`**:
  - title, a one-paragraph purpose;
  - **legal notice**: passive reception only; laws on RF monitoring, logging
    and using Remote ID data vary by country; never transmit against aircraft;
  - supported hardware with a link to the LilyGO MVSRLora repo;
  - feature list with confidence levels and the spec's non-goals stated plainly;
  - build/flash (`pio run -e t-display-s3-pro-lr1121 -t upload`; hold BOOT if
    the port does not appear);
  - controls table; serial commands; CSV format; running tests
    (`pio test -e native`).
  - Remove every reference to the D-duino, CC1101, NRF24, PN5180, IR,
    jamming, deauth and cloning. Keep the logo image.
- [ ] **Step 2: Finalize `docs/FIELD_TEST.md`**: an ordered checklist combining
  the sections from Tasks 9–15, with an expected-result column.
- [ ] **Step 3: Verify**:
  - `pio test -e native`: all suites PASS (count listed in the commit message);
  - `pio run -e t-display-s3-pro-lr1121`: `[SUCCESS]`, RAM/flash usage noted;
  - `grep -rn "transmit\|setOutputPower\|80211_tx" src/`: no matches.
- [ ] **Step 4: Commit** `docs: README and field test checklist for NETH3T detector`.
  Push to `claude/jolly-bell-ky5fra`.
