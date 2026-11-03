# NETH3T — Built-in Self-Test and Landscape Spectrum Analyzer (Design)

Date: 2026-11-03
Status: Draft for review
Builds on: `2026-11-03-neth3t-sigint-design.md`

## 1. Goals

1. **Built-in self-test:**
   - Checks, on the device itself and without transmitting anything, that every radio is alive and that every detector produces the right target, alert and log row.
   - Ends with a pass/warn/fail report.
2. **Landscape spectrum analyzer:**
   - The Spectrum tab turns sideways (480×222) to show a live trace with peak hold and the noise floor, frequency labels, and a waterfall strip.
   - Tap to read a frequency and its level; zoom in and out around that point.

### Non-goals

- No transmitting. A second-board test beacon is out of scope here.
- No pinch zoom: the touch controller's multi-touch is unreliable at this size, so zoom uses on-screen buttons and the hardware buttons.
- No antenna or RF-path verification. That still needs a real signal source (see FIELD_TEST.md).

## 2. Self-test

### 2.1 Entry points

- Settings → new row **Run self-test**.
- Serial command `selftest`.
- Both run the same test. The device keeps scanning normally while it runs.

### 2.2 Checks

Each check returns PASS, WARN or FAIL with a one-line detail.

| # | Check | Method | FAIL / WARN rule |
|---|---|---|---|
| H1 | Boot peripherals | `g_boot` flags: PMU, display, touch, RTC, LR1121 | FAIL if display or LR1121 failed; WARN for PMU, touch, RTC |
| H2 | SD card | `sd_log::active()` | WARN if no card |
| H3 | LR1121 version | `getVersionInfo` succeeded at boot | FAIL if unknown |
| H4 | LR1121 RSSI per band | Latest sweep row for each band is less than 10 s old and every bin is in −135…−20 dBm, with at least one bin not equal to the others | FAIL if a band has no recent sweep or values are stuck or out of range |
| H5 | LR1121 CAD path | `stats().cad_hits + cad_free_count` increases within 10 s (new counter `cad_runs`) | FAIL if no CAD completed |
| H6 | LR1121 error rate | `errors / (cad_runs + sweeps)` < 5 % over the test | WARN at ≥ 5 %, FAIL at ≥ 50 % |
| H7 | Wi-Fi receive | Promiscuous frame counter (new: `wifi_sniffer::frames()`) increases within 5 s | WARN if 0 (no access points nearby is possible) |
| H8 | BLE receive | Advert counter (new: `ble_scanner::adverts()`) increases within 5 s | WARN if 0 |
| P1 | Remote ID over Wi-Fi beacon | Golden beacon frame passes through the **same** function the sniffer uses → `db::post` | FAIL unless a target with the expected serial, lat/lon (±1e-6) and pilot position appears within 1 s |
| P2 | Remote ID over Wi-Fi NAN | Golden NAN frame, same path | FAIL as P1 |
| P3 | Remote ID over BLE (legacy, two adverts: Basic ID then Location) | Same function the BLE scanner uses | FAIL unless they merge into **one** target with serial + position |
| P4 | Drone-maker Wi-Fi | Golden beacon with SSID `TELLO-SELFTEST` | FAIL unless a DRONE_WIFI target with vendor "Ryze/DJI Tello" appears |
| P5 | ELRS fingerprint | A golden CAD-hit sequence (SF7/BW500, 6 hits on 4 channels) goes into a **separate** `RcFingerprint` instance → `db::post` | FAIL unless target `ELRS900-SF7` with rate 100 Hz appears |
| P6 | Meshtastic | Golden 16-byte header + payload through the same function the scanner uses for received packets | FAIL unless `!5e1f7e57` appears |
| P7 | LoRaWAN | Golden unconfirmed-uplink frame, same path | FAIL unless `DA:26011BDA` appears |
| P8 | Alert policy | `alert_for` on the P1, P4 and P6 targets | FAIL unless they give ALARM, NOTICE and NONE respectively |
| P9 | CSV row | `csv_row` on the P1 observation | FAIL unless the row contains the serial and `lat` with 7 decimals |
| M1 | Speaker and vibration | Plays the alert chirp and the double vibration once | Always "CHECK": the user confirms by ear and feel |

### 2.3 Test targets

- The runner prefixes every injected observation's id with `TEST-` after building it, so `!5e1f7e57` becomes `TEST-!5e1f7e57`. The ids in the table above are given without the prefix.
- Test targets **never** trigger alerts, never go to the SD log or serial `LOG,` lines, and are removed from the target table when the test ends (new `db::remove(type, id)`). They show in the Targets list during the test, labelled with the `TEST-` prefix.
- P1–P9 share the frame-to-observation code with the live radios, so the self-test exercises the real decoding path rather than a copy of it.

### 2.4 Refactor to make that possible

The frame-to-observation logic moves out of the radio drivers into a pure module, `src/detect/obs_build.{h,cpp}`:

```c++
struct DetectFlags { bool rid_wifi, rid_ble, drone_wifi; };
bool obs_from_wifi(const uint8_t* frame, size_t len, int8_t rssi, uint8_t ch, uint32_t now,
                   const DetectFlags& f, IdCache& ids, Observation& out);
bool obs_from_ble(const uint8_t mac[6], const uint8_t* adv, size_t len, int8_t rssi, uint32_t now,
                  const DetectFlags& f, IdCache& ids, Observation& out);
void obs_from_lora_packet(PlanKind kind, uint32_t freq_khz, uint8_t sf, uint32_t bw_hz,
                          const uint8_t* buf, size_t len, float rssi, float snr, uint32_t now, Observation& out);
```

- `wifi_sniffer`, `ble_scanner` and `lora_scanner` call these.
- The golden vectors live in `src/selftest/vectors.{h,cpp}` (pure, `TEST-` ids).
- The host test suite runs every vector through `obs_build` and checks the same expected values the on-device test checks, so the vectors themselves are verified on the PC.

### 2.5 Report

- **Screen:** a results list showing a coloured status and a one-line detail per check, plus a summary of how many passed, warned and failed. Back or Home closes it.
- **Serial:** `SELFTEST,<id>,<PASS|WARN|FAIL|CHECK>,<detail>` per check, then `SELFTEST,SUMMARY,…`.
- **Duration:** about 12 s, mostly waiting for the radio counters. A progress line shows the current check.

## 3. Landscape spectrum analyzer

### 3.1 Orientation and input

- The Spectrum screen draws with the canvas rotated to landscape (480×222). The portrait screens, flush and buffer are unchanged; the rotation applies only while that screen is drawn.
- Touch coordinates are rotated to match by a pure helper, `touch_to_landscape(px, py) → (lx, ly)`. The rotation direction is a single constant, `kSpectrumRotation` (1 or 3), so it can be flipped if the field test shows the picture upside down.
- In landscape the bottom tab bar is hidden.
  - **Leave:** Back button top-right, the Home key, or BOOT.
  - **Top-bar touch buttons:** `Band`, `−` (zoom out) and `+` (zoom in).
  - **Hardware buttons:** up = zoom in, down = zoom out, long-press down = next band.

### 3.2 Layout (landscape 480×222)

| y (px) | Content |
|---|---|
| 0–21 | Top bar: band name, span, step, noise floor, battery; `Band`, `−`, `+`, `Back` |
| 22–149 | Trace area, −130…−30 dBm, grid every 10 dB with labels at the left. Live trace in amber; peak hold in red, decaying 1 dB/s; noise floor as a blue line; CAD hits as white ticks labelled `SFn` |
| 150–161 | Frequency axis: start, centre and stop labels |
| 162–221 | Waterfall: the last 60 sweeps, newest on top, same colour ramp as before |

- **Marker:** tapping the trace area places a vertical marker. The readout `915.200 MHz  −72 dBm  (peak −61)` is drawn in the top-left of the trace area.

### 3.3 Zoom

- Zoom is centred on the marker, or on the span centre if there is no marker.
  - `+` halves the span.
  - `−` doubles it, up to the full band.
- **Minimum span:** 4 MHz sub-GHz (50 kHz steps); 16 MHz at 2.4 GHz (200 kHz steps). Below those the receiver's own filter width makes finer steps meaningless.
- The zoomed window is clamped inside the band (902–928 / 863–870 / 433.05–434.79 / 2400–2480 MHz). The 433 band (1.74 MHz wide) cannot zoom.
- Points per sweep stay at 80 at most (unchanged), so a zoomed sweep takes about as long as a full one.
- **New scanner API:**
  - `lora_scanner::set_focus(uint8_t band, uint32_t f0_khz, uint32_t f1_khz)`: while the Spectrum screen is open, the scheduler sweeps the viewed band on **every other** plan visit (normal scanning continues in between), using the focused range.
  - `clear_focus()` restores normal scheduling.
  - The waterfall and peak hold reset whenever the range changes.
- **Detection while zoomed:** hot bins from a zoomed sweep still feed the FHSS detector. The FHSS rule needs 6 distinct frequencies, so a narrow zoom may stop FHSS detection. That is documented and accepted.

### 3.4 Pure, host-tested pieces (`src/ui/spectrum_view.{h,cpp}`)

- `Span zoom_in(Span s, uint32_t center, Band b)` and `Span zoom_out(Span s, uint32_t center, Band b)`: clamping, minimum span, no-op at the limits.
- `int freq_to_x(uint32_t f, Span s, int width)` and `uint32_t x_to_freq(int x, Span s, int width)`: they round-trip within one step.
- `PeakHold::update(const int8_t* dbm, uint8_t n, uint32_t now)`: per-bin maximum with 1 dB/s decay; resets on a range change.
- `touch_to_landscape`: maps the four corners correctly for both rotation constants.
- `make_sweep_plan(f0, f1, min_step, hf)` (in `channel_plans`): at most 80 points, endpoints included.

## 4. Error handling

- If the LR1121 failed at boot, the self-test reports H3–H6 as FAIL and still runs every other check. The Spectrum screen says "LoRa radio not available".
- The self-test can't run twice at once; a second request is ignored with a serial note.
- If an injected target doesn't appear within 1 s (db queue full), that check FAILs with "not seen in table (queue full?)". It doesn't hang.
- Leaving Spectrum always calls `clear_focus()`, including through Home, Back or an alert-banner tap.

## 5. Testing

- **Host:** new suites `test_obs_build` (every golden vector gives the expected observation) and `test_spectrum_view`. Existing suites keep passing.
- **Build:** the firmware compiles cleanly.
- **Device:** FIELD_TEST.md gets two new sections.
  - **Self-test:** every P-check passes; H7/H8 pass indoors near Wi-Fi/phones; M1 chirps and buzzes.
  - **Spectrum:** landscape orientation is correct; tap shows a marker; `+` and `−` change the span; a 433 MHz fob press shows on the trace.
