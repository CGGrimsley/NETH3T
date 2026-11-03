# Self-Test and Landscape Spectrum Analyzer — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an on-device, receive-only self-test and a landscape spectrum analyzer with zoom to the NETH3T firmware.

**Architecture:**
- Frame-to-observation logic moves into a pure module, `obs_build`. Both the live radios and the self-test's golden vectors use it, so the self-test exercises the real decoding path.
- Spectrum geometry (zoom, frequency↔pixel mapping, peak hold, touch rotation) is pure and host-tested.
- The scanner gains a focus mode that re-sweeps the viewed band.

**Tech Stack:** unchanged from `docs/superpowers/plans/2026-11-03-neth3t-sigint.md` (PlatformIO `espressif32 @ 7.1.3`, RadioLib 7.8.1, GFX 1.4.9, Unity native tests).

**Spec:** `docs/superpowers/specs/2026-11-03-selftest-and-spectrum-design.md`

## Global Constraints

- **Receive-only.** No new code calls `transmit`, `startTransmit`, `esp_wifi_80211_tx`, BLE advertising or `setOutputPower`.
- **Pure-module rule** (as before): `src/detect/*`, `src/selftest/vectors.*`, `src/selftest/odid_encode.h` and `src/ui/spectrum_view.*` use no Arduino, ESP-IDF or FreeRTOS headers.
- **`TEST-` ids.** Test targets use the `TEST-` id prefix. They never alert, never reach the SD log or the `LOG,` serial lines, and are removed after the run.
- **Builds.** `pio test -e native` and `pio run -e t-display-s3-pro-lr1121` pass at the end of every task.
- **Commits** end with the session trailers. No model identifiers.

## Review Focus

1. **Leaving Spectrum by any route** (Home, Back, BOOT, banner tap) must call `clear_focus()`, otherwise normal scanning stays degraded. Owner: Task 6, the `leave_spectrum()` helper used by all exits. Not host-testable; build check plus FIELD_TEST step.
2. **Zoom at the band edges** (marker at 902.0 MHz, or in the 433 band, which is narrower than the minimum span) must clamp and never produce f0 ≥ f1. Owner: `test_zoom_clamps_at_band_edge` and `test_433_cannot_zoom` (Task 3).
3. **Self-test while a target is locked or the table is full.** Injection must not steal the lock, and a full queue gives a FAIL detail, not a hang. Owner: the 1 s wait-with-timeout in Task 5; `test_remove_keeps_lock_of_other_target` (Task 4).
4. **Golden-vector drift.** If a decoder changes, the vectors' expected values must be re-checked on the host. Owner: `test_obs_build` runs every vector (Task 2).
5. **Peak hold after a range change** must reset, not show peaks from the old span. Owner: `test_peak_resets_on_range_change` (Task 3).

---

### Task 1: `obs_build` module and radio refactor

**Files:**
- Create: `src/detect/obs_build.{h,cpp}`, `test/test_obs_build/test_main.cpp`
- Modify: `src/radio/wifi_sniffer.cpp`, `src/radio/ble_scanner.cpp`, `src/radio/lora_scanner.cpp`; delete `src/radio/rid_common.h` (its logic moves into `obs_build`)

**Interfaces:**
- Produces (spec §2.4):
  - `struct DetectFlags { bool rid_wifi, rid_ble, drone_wifi; };`
  - `bool obs_from_wifi(const uint8_t* frame, size_t len, int8_t rssi, uint8_t ch, uint32_t now, const DetectFlags&, IdCache&, Observation&)`
  - `bool obs_from_ble(const uint8_t mac[6], const uint8_t* adv, size_t len, int8_t rssi, uint32_t now, const DetectFlags&, IdCache&, Observation&)`
  - `void obs_from_lora_packet(PlanKind, uint32_t freq_khz, uint8_t sf, uint32_t bw_hz, const uint8_t* buf, size_t len, float rssi, float snr, uint32_t now, Observation&)`
- Behaviour: identical to the current `handle()` / `onResult` / `post_packet` code. Wi-Fi `freq_khz = 2407000 + ch×5000`; BLE `freq_khz = 2402000`.

- [ ] **Step 1: Write the failing tests** in `test/test_obs_build`. Build frames with `test/helpers/odid_encode.h`.
  - `test_wifi_odid_beacon_to_remoteid`: type REMOTEID_WIFI, DECODED, id = serial, rssi and freq set.
  - `test_wifi_flags_disable`: `rid_wifi=false` gives false.
  - `test_wifi_tello_ssid_to_drone_wifi`: vendor "Ryze/DJI Tello", label = SSID.
  - `test_wifi_plain_router_ignored`.
  - `test_wifi_dji_legacy`.
  - `test_ble_basic_then_location_same_id`: the second observation's id = serial via `IdCache`.
  - `test_lora_mesh_packet`: id `!5e1f7e57`, label `Mesh !5e1f7e57`.
  - `test_lora_lorawan_packet`: id `DA:26011BDA`.
  - `test_lora_garbage_unknown`: LORA_UNKNOWN, id `LORA-915-SF7`.
- [ ] **Step 2: Run** `pio test -e native -f test_obs_build`: FAIL (undefined).
- [ ] **Step 3: Implement** `obs_build.cpp` by moving the existing logic. Make the radios call it: they map `settings::get()` to `DetectFlags`, then call `db::post`.
- [ ] **Step 4: Run** native: all PASS. Build firmware: SUCCESS.
- [ ] **Step 5: Commit** `refactor(detect): shared frame-to-observation module`.

### Task 2: Golden self-test vectors

**Files:**
- Create: `src/selftest/odid_encode.h` (moved from `test/helpers/odid_encode.h`, which becomes `#include "selftest/odid_encode.h"`), `src/selftest/vectors.{h,cpp}`, `test/test_vectors/test_main.cpp`
- Modify: `platformio.ini` native filter, adding `+<selftest/vectors.cpp>`

**Interfaces:**
- Produces:
```c++
struct Vec { const char* name; size_t len; uint8_t data[200]; };
void vec_rid_beacon(Vec&);          // serial "1596F0SELFTEST01", drone 35.7796,-78.6382 alt 120, pilot 35.7790,-78.6390
void vec_rid_nan(Vec&);             // same serial/positions as the beacon
void vec_rid_ble_basic(Vec&);       // legacy advert, Basic ID, serial "1596F0SELFTEST02"
void vec_rid_ble_location(Vec&);    // legacy advert, Location 35.7800,-78.6400
void vec_tello_beacon(Vec&);        // SSID "TELLO-SELFTEST", MAC 02:00:00:00:00:01
void vec_mesh_packet(Vec&);         // sender 0x5e1f7e57, 16-byte header + 8 payload bytes
void vec_lorawan_uplink(Vec&);      // 40 DA 1B 01 26 80 07 00 ... (DevAddr 0x26011BDA, FCnt 7), 16 bytes
size_t vec_elrs_hits(CadHit* out, size_t max, uint32_t t0);  // 6 SF7/BW500 hits on 903.5/910.1/915.5/922.1 MHz over 600 ms
extern const uint8_t kVecBleMac[6]; // 02:00:00:00:00:02
```

- [ ] **Step 1: Write the failing tests** in `test/test_vectors`: run each vector through `obs_build` (and the ELRS vector through a fresh `RcFingerprint`). Assert exactly the spec §2.2 expectations: serial, lat/lon ±1e-6, pilot position, vendor, ids, ELRS rate 100.
- [ ] **Step 2: Run**: FAIL. Implement `vectors.cpp` with the encoder. Run: PASS. Move the encoder and check the old suites still PASS.
- [ ] **Step 3: Commit** `feat(selftest): golden vectors verified on host`.

### Task 3: Spectrum view math and sweep-plan builder

**Files:**
- Create: `src/ui/spectrum_view.{h,cpp}`, `test/test_spectrum_view/test_main.cpp`
- Modify: `src/detect/channel_plans.{h,cpp}` (expose `make_sweep_plan`), `test/test_channel_plans`, `platformio.ini` native filter (`+<ui/spectrum_view.cpp>`)

**Interfaces:**
```c++
struct Span { uint32_t f0_khz, f1_khz; };
struct BandLimits { uint32_t lo_khz, hi_khz, min_span_khz; };
BandLimits band_limits(uint8_t band, Region r);  // 0: 902000-928000 / 863000-870000, min 4000; 1: 433050-434790, min 4000; 2: 2400000-2480000, min 16000
Span zoom_in(Span s, uint32_t center_khz, const BandLimits& b);   // halves span, clamps
Span zoom_out(Span s, uint32_t center_khz, const BandLimits& b);  // doubles span, clamps to band
int freq_to_x(uint32_t f_khz, Span s, int width);
uint32_t x_to_freq(int x, Span s, int width);
class PeakHold { public: void reset(); void update(const int8_t* dbm, uint8_t n, uint32_t now_ms); int8_t at(uint8_t i) const; uint8_t n() const; };  // 1 dB/s decay
void touch_to_landscape(int16_t px, int16_t py, uint8_t rotation, int16_t* lx, int16_t* ly);  // portrait 222x480 -> landscape 480x222
ScanPlan make_sweep_plan(const char* name, uint32_t f0_khz, uint32_t f1_khz, uint32_t min_step_khz, bool hf);  // <=80 pts
```

- [ ] **Step 1: Write the failing tests**:
  - `test_zoom_in_halves_around_center`.
  - `test_zoom_in_respects_min_span`.
  - `test_zoom_clamps_at_band_edge`: center 902000 gives f0 = 902000 and f1 > f0.
  - `test_zoom_out_to_full_band`.
  - `test_433_cannot_zoom`: zoom_in returns the full band.
  - `test_freq_x_roundtrip`: within one pixel's worth of frequency.
  - `test_peak_hold_max_and_decay`: −60 then −80 after 5 s gives −65.
  - `test_peak_resets_on_range_change`: `reset()` then update gives the new values.
  - `test_touch_to_landscape_corners`: for rotation 1 and rotation 3.
  - `test_make_sweep_plan_points`: 4 MHz at a 50 kHz minimum step gives 80 points ending at f1; 26 MHz is capped at 80.
- [ ] **Step 2: Run**: FAIL. Implement. Run: PASS.
- [ ] **Step 3: Commit** `feat(ui): spectrum zoom/peak/rotation math`.

### Task 4: Radio counters, `db::remove`, `TEST-` suppression, scanner focus

**Files:**
- Modify: `src/core/target_db.{h,cpp}` (add `bool remove(SourceType, const char*)`), `test/test_target_db`, `src/core/db_task.{h,cpp}` (`db::remove`), `src/radio/wifi_sniffer.{h,cpp}` (`uint32_t frames()`), `src/radio/ble_scanner.{h,cpp}` (`uint32_t adverts()`), `src/radio/lora_scanner.{h,cpp}` (`Stats` gains `cad_runs`, `sweeps`; `set_focus`, `clear_focus`), `src/ui/ui.cpp` (alert loop skips `TEST-`), `src/log/sd_log.cpp` (skips `TEST-`)

**Interfaces:**
- `void lora_scanner::set_focus(uint8_t band, uint32_t f0_khz, uint32_t f1_khz)`: the scheduler runs a focus sweep built with `make_sweep_plan` on every other visit. Minimum step: 50 kHz sub-GHz, 200 kHz HF.
- `void lora_scanner::clear_focus()`.
- `bool is_test_id(const char* id)` in `target_db.h`: id starts with `TEST-`.

- [ ] **Step 1: Write the failing tests**: `test_remove_target`, `test_remove_keeps_lock_of_other_target`, `test_remove_locked_unlocks`, `test_is_test_id`.
- [ ] **Step 2: Run**: FAIL. Implement. Run: PASS. Build: SUCCESS.
- [ ] **Step 3: Commit** `feat: radio counters, db remove, TEST- suppression, scanner focus`.

### Task 5: Self-test runner, report screen, entry points

**Files:**
- Create: `src/selftest/selftest.{h,cpp}`, `src/ui/screen_selftest.cpp`
- Modify: `src/ui/ui.h` (`Screen::SELFTEST`), `src/ui/ui.cpp` (route), `src/ui/ui_internal.h`, `src/ui/screen_settings.cpp` (row **Run self-test**), `src/log/serial_cmd.cpp` (`selftest`)

**Interfaces:**
```c++
enum class StResult : uint8_t { PENDING, PASS, WARN, FAIL, CHECK };
struct StItem { const char* id; const char* name; StResult r; char detail[40]; };
namespace selftest {
bool start();               // false if already running; runs in its own task
bool running();
size_t items(StItem* out, size_t max);   // snapshot for the screen
const char* progress();     // current check name
}
```
- Checks H1–H8, P1–P9 and M1 exactly as in the spec §2.2 table.
  - Injection: build the observation through `obs_build` or `RcFingerprint`, prefix its id with `TEST-`, then `db::post`.
  - Wait up to 1 s (polling `db::snapshot`) for the target to appear.
  - Remove all `TEST-` targets at the end.
- Serial lines `SELFTEST,<id>,<result>,<detail>` and `SELFTEST,SUMMARY,pass=..,warn=..,fail=..`.

- [ ] **Step 1: Implement**: runner, screen (coloured list, summary, Back/Home), settings row, serial command.
- [ ] **Step 2: Build**: SUCCESS. Native: PASS. Then `grep` for transmit calls: none.
- [ ] **Step 3: Commit** `feat(selftest): on-device self-test with report screen`.

### Task 6: Landscape spectrum analyzer screen

**Files:**
- Modify: `src/ui/screen_spectrum.cpp` (rewrite), `src/ui/ui.cpp` (no tabs or status bar on Spectrum; Home, banner and leave paths call `ui_spectrum::leave()`), `src/ui/ui_internal.h` (`ui_spectrum::leave()`)

**Interfaces:**
- Consumes: `spectrum_view` (Task 3), `lora_scanner::set_focus/clear_focus/latest_sweep/recent_cad` (Task 4).
- `constexpr uint8_t kSpectrumRotation = 1;` (spec §3.1).

Layout and controls follow spec §3.2–3.3 exactly:
- **Draw:** `canvas->setRotation(kSpectrumRotation)` for the frame, then `setRotation(0)` afterwards.
- **Top-bar touch zones** (landscape x): `Band` 250–310, `−` 316–356, `+` 362–402, `Back` 410–476, all at y 0–21.

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Build**: SUCCESS. Native: PASS.
- [ ] **Step 3: Commit** `feat(ui): landscape spectrum analyzer with marker and zoom`.

### Task 7: Docs and final review

- [ ] **Step 1:** README (self-test, spectrum controls, `selftest` command) and FIELD_TEST sections "Self-test" and "Spectrum analyzer".
- [ ] **Step 2:** Run native tests and the firmware build. Commit `docs: self-test and spectrum analyzer`. Push.
- [ ] **Step 3:** Fresh whole-branch review of this plan's commit range; one fix pass.
