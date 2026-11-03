# NETH3T — Handheld Anti-Drone & LoRa SIGINT Detector (Design)

Date: 2026-11-03
Status: Draft for review

## 1. Purpose

Turn a LilyGO **T-Display-S3-Pro with the MVSRLora back plate (LR1121)** into a
handheld, **receive-only** detector for drones, RC control links and LoRa
devices, built for *hunting*: see a target, lock it, walk toward it guided by
a live RSSI meter, audio and vibration.

### Success criteria

1. Flash with PlatformIO; the device boots to a live target list.
2. A phone running an OpenDroneID transmitter app (or a real Remote ID drone)
   appears as a **Remote ID drone** with serial, drone position and pilot
   position.
3. A powered ExpressLRS transmitter (900 MHz or 2.4 GHz LoRa modes) appears as
   an **ELRS link** with band and estimated packet rate.
4. Meshtastic and LoRaWAN nodes appear with their node ID / DevAddr.
5. Tapping any target opens Hunt mode with RSSI updates several times per second,
   a trend arrow, peak hold, and RSSI-scaled audio/vibration.
6. Host unit tests for all parsers/classifiers pass; firmware builds cleanly.

### Non-goals (v1)

- **No transmitting against targets**: no jamming, spoofing, deauth or replay.
  The device never keys its transmitter during normal use.
- No decryption of Meshtastic/LoRaWAN payloads or ELRS packets.
- No DJI OcuSync/O3/O4 demodulation (wideband OFDM, not possible on this hardware);
  DJI aircraft are detected through Remote ID.
- No own-position GPS, no bearing/compass, no PC companion app.
- ELRS FLRC modes (2.4 GHz F500/F1000) are invisible to the LR1121 and out of scope.

## 2. Hardware

| Function | Part | Pins (ESP32-S3) |
|---|---|---|
| MCU | ESP32-S3, 16 MB flash, 8 MB OPI PSRAM | — |
| Display | ST7796, 222×480, col offset 49 | SPI SCK 18 / MOSI 17 / MISO 8, CS 39, DC 9, RST 47, BL 48 |
| Touch | CST226SE (I²C) | SDA 5 / SCL 6, INT 21, RST 13 |
| PMU / charger | SY6970 (I²C 0x6A) | SDA 5 / SCL 6 |
| SD card | SPI | CS 14 (shared SPI bus) |
| LoRa | LR1121 (sub-GHz + 2.4 GHz LoRa) | CS 7, IRQ 40, RST 10, BUSY 46 (shared SPI bus) |
| LR1121 RF switch | DIO5/DIO6 | STBY 00, RX 10, TX 01, TX_HP 01, TX_HF 00 |
| Back-plate power | RT9080 LDO | EN 42 → must be driven HIGH first |
| Speaker | MAX98357A I²S amp | BCLK 4, LRCLK 15, DATA 11, SD_MODE 41 |
| Vibration motor | PWM | 45 |
| RTC | PCF85063 (I²C) | INT 21 |
| Buttons | 3 user buttons | 0 (BOOT, left-bottom), 12 (right-top), 16 (right-bottom) |

Source: LilyGO `T-Display-S3-Pro` and `T-Display-S3-Pro-MVSRLora` repos
(`pin_config.h`, LR1121 examples). The camera pin map conflicts with the back
plate and is unused.

**Shared SPI bus.** Display, SD and LR1121 share one SPI bus. A single
`spi_bus` mutex guards every transaction; the display is redrawn in bounded
chunks so LR1121 scans are never starved for more than ~10 ms.

**RTC bonus.** The PCF85063 gives real timestamps when set (from the PC over
serial: `time <unix>`); otherwise logs use seconds since boot.

## 3. Software stack

- PlatformIO, `espressif32` platform, Arduino framework (versions pinned in
  `platformio.ini`).
- **RadioLib** — LR1121 driver (LoRa RX, CAD, instantaneous RSSI).
- **Arduino_GFX** — ST7796 display (as in LilyGO's examples), drawn into a
  PSRAM canvas and flushed in strips.
- **SensorLib** (lewisxhe) — CST226SE touch, PCF85063 RTC.
- **XPowersLib** (lewisxhe) — SY6970 battery status.
- **NimBLE-Arduino** — BLE 4 legacy + BLE 5 coded-PHY extended scanning.
- ESP-IDF Wi-Fi promiscuous API (through Arduino) for 802.11 capture.
- ESP32 I²S driver for speaker tones.

## 4. Architecture

```
include/board_pins.h           all pins (from §2)
src/main.cpp                   bring-up order, task creation
src/core/observation.h         Observation struct (pure)
src/core/target_db.{h,cpp}     merge / smooth / trend / age (pure, mutex wrapper)
src/core/scheduler.{h,cpp}     radio time slicing + hunt lock
src/core/spi_bus.{h,cpp}       shared-bus mutex
src/detect/odid.{h,cpp}        OpenDroneID decoder (pure)
src/detect/wifi_frames.{h,cpp} 802.11 beacon/NAN/vendor-IE parsing (pure)
src/detect/ble_adv.{h,cpp}     BLE AD parsing for Remote ID (pure)
src/detect/drone_vendors.{h,cpp} OUI + SSID drone tables (pure)
src/detect/lora_headers.{h,cpp}  Meshtastic + LoRaWAN header parsing (pure)
src/detect/rc_fingerprint.{h,cpp} ELRS / FHSS classifiers (pure)
src/radio/wifi_sniffer.cpp     promiscuous capture + channel hopping
src/radio/ble_scanner.cpp      NimBLE scanning
src/radio/lora_scanner.cpp     LR1121 sweep / CAD / RX
src/ui/{display,touch,buttons,screens_*}.cpp
src/alert/{audio,vibe,alerts}.cpp
src/log/sd_log.cpp             CSV logger + serial mirror
src/settings/settings.cpp      NVS-backed settings
test/                          Unity tests run with `pio test -e native`
```

Every module marked *pure* has no Arduino or ESP-IDF includes, so it compiles
on the host for tests.

### Data flow

Radio tasks → parsers/classifiers → `Observation` → FreeRTOS queue →
`target_db` task (merge) → read-only snapshots for UI, alerts and logger.

```c++
struct Observation {
  SourceType type;        // REMOTEID_WIFI, REMOTEID_BLE, DRONE_WIFI, ELRS, FHSS,
                          // MESHTASTIC, LORAWAN, LORA_UNKNOWN
  Confidence conf;        // DECODED, FINGERPRINT, HEURISTIC
  char id[24];            // serial / MAC / node id / DevAddr / synthetic
  uint32_t freq_khz;      // or Wi-Fi channel * 1000 for 2.4 GHz Wi-Fi/BLE (ch 37-39)
  int16_t rssi_dbm; int8_t snr_db;
  uint32_t t_ms;
  Extra extra;            // tagged union: ODID fields | LoRa SF/BW | vendor name | ...
};
```

### Target database

- Fixed size: 128 targets. When full, evict the lowest confidence first, then
  the oldest.
- **Merge key**: (type, id). ELRS/FHSS IDs are synthetic, made from band +
  modulation, because those links carry no readable address.
- Per target, keep a 64-entry RSSI ring buffer plus an EMA (α = 0.3).
- **Trend**: EMA now vs EMA 3 s ago; within ±2 dB is "steady".
- Peak hold, first-seen time, last-seen time, hit count.
- **Ageing**: dimmed after 20 s with no hits, removed after 60 s
  (a locked target is never removed).
- ODID fields from separate messages (Basic ID, Location, System, …) are merged
  into one drone record.

### Scheduler

**2.4 GHz loop (ESP32 radio)**
- Wi-Fi hop sequence: 1, 6, 11, then the other channels. Dwell is 120 ms,
  except 250 ms on channel 6 (NAN).
- BLE scan window: 300 ms out of every 1 s, coded PHY and 1M PHY on alternate
  windows.

**LR1121 loop**: a list of *scan plans* worked through in turn. Each plan has
a band, a channel list, the SF/BW pairs to try, and a mode (RSSI sweep, CAD,
RX with timeout).

| Plan | Mode | Frequencies | Settings | Detects |
|---|---|---|---|---|
| ISM sweep 915/868/433 | RSSI sweep | 200 kHz steps over the region band | — | spectrum view, FHSS |
| ELRS 900 | CAD | ELRS channel plan for the region | BW500, SF6–SF9 | ELRS 900 |
| ELRS 2.4 | CAD | 80 ch, 2400.4–2479.4 MHz | BW812.5, SF5–SF8 | ELRS 2.4 |
| Meshtastic | RX, 1.5 s | region default + LongFast slot | BW250 SF11, sync word 0x2B (plus the other presets in turn) | Meshtastic |
| LoRaWAN | RX, 1 s | US915 sub-band 2 / EU868 default channels | BW125 SF7–SF10, sync word 0x34 | LoRaWAN |

- A **region** setting (FCC or EU) picks the channel plans.
- Plans are interleaved so that each one is revisited within about 4 s.

**Hunt lock**
- Wi-Fi/BLE target: the hopper pins that channel, or runs BLE scanning
  continuously.
- LoRa target: the LR1121 runs only that target's plan (CAD/RX on its
  frequency and SF/BW, or a fast RSSI sweep around its frequency for FHSS).
- RSSI updates at ≥ 4 Hz for Wi-Fi/LoRa RX targets; ELRS targets update at
  their hop-hit rate.

## 5. Detectors

### 5.1 Remote ID over Wi-Fi

**Beacon Remote ID**
- Look for vendor-specific IE 221 with OUI `FA:0B:BC` and OUI type `0x0D`.
- Then a 1-byte message counter, then an ODID Message Pack.

**NAN Remote ID**
- Action frames (category `0x04`, vendor-specific) sent to
  `51:6F:9A:01:00:00`, with Wi-Fi Alliance OUI `50:6F:9A`, type `0x13`.
- The service descriptor attribute whose service ID equals the first 6 bytes of
  SHA-256(`org.opendroneid.remoteid`) carries the ODID message pack in its
  service info.

**Legacy DJI beacons**
- DJI's vendor IE with OUI `26:37:12`, best effort.
- Parse serial and positions where the format is known; otherwise report
  "DJI DroneID (undecoded)" as a FINGERPRINT.

**Every frame**
- RSSI from the promiscuous metadata; ID is the ODID serial if present,
  otherwise the source MAC.

### 5.2 Remote ID over BLE

- **Legacy adverts**: AD type `0x16` (service data), UUID `0xFFFA`, app code
  `0x0D`, then counter and one 25-byte ODID message.
- **Extended adverts** (BLE 5, coded PHY): the same AD structure, but they may
  carry a full message pack.
- Merging by serial number joins a drone's BLE and Wi-Fi sightings into one
  target.

### 5.3 OpenDroneID decoder (`odid`, pure)

- Implements ASTM F3411-22a / ASD-STAN prEN 4709-002 message types:

| Type | Message | Fields |
|---|---|---|
| 0x0 | Basic ID | ID type, UA type, 20-character ID |
| 0x1 | Location/Vector | status, direction, speeds, lat/lon (1e-7 deg), pressure/geodetic/height altitudes (0.5 m, −1000 m offset), accuracies, timestamp |
| 0x2 | Authentication | flagged only, not verified |
| 0x3 | Self-ID | text |
| 0x4 | System | operator lat/lon, area info, operator altitude, classification |
| 0x5 | Operator ID | operator ID |
| 0xF | Message Pack | message size 25, count ≤ 9 |

- Hardening:
  - Every field is bounds-checked; an unknown message type is skipped.
  - Packs with a wrong message size or an impossible count are rejected.
  - Lat/lon outside ±90/±180 are marked invalid.

### 5.4 Drone-maker Wi-Fi

- A table of OUI prefixes for DJI, Parrot, Autel, Skydio, Yuneec, Ryze/Tello,
  Hubsan, Holy Stone and similar makers, checked against beacon and probe-response
  source addresses.
- An SSID pattern table: `TELLO-`, `DJI-`, `Mavic`, `Spark-`, `Phantom`,
  `Anafi`, `Bebop`, `Disco`, `Autel`, `EVO`, `Skydio`, `Yuneec` and similar,
  case-insensitive prefix/contains.
- Result: confidence HEURISTIC, with the vendor name shown.
- The OUI list is checked against the IEEE registry during implementation, and
  each entry carries a source comment.

### 5.5 LoRa scanning

**RSSI sweep**
- Put the radio in RX on each step, read instantaneous RSSI, and keep a
  per-band noise floor (running 20th percentile).
- A bin more than 10 dB above the floor counts as "hot".
- Hot bins feed both the spectrum waterfall and the FHSS classifier.

**CAD**
- Run RadioLib `scanChannel()` with a CAD config for each frequency and SF.
- A positive CAD becomes a hit `{freq, sf, bw, t}`; for RSSI, follow it with a
  short RX to read packet RSSI/SNR when possible.

**Meshtastic header** (16 bytes, unencrypted)
- dest (4), sender (4), packet ID (4), flags (1), channel hash (1),
  next-hop (1), relay (1).
- ID is `!%08x` of the sender.

**LoRaWAN header**
- MHDR (MType), plus for data frames DevAddr (4, little-endian), FCtrl, FCnt.
- Join-request frames give JoinEUI/DevEUI.
- ID is the DevAddr or DevEUI.

### 5.6 RC link fingerprinting (`rc_fingerprint`, pure)

The input is a time-ordered stream of CAD hits and hot RSSI bins.

**ELRS 900**
- Within a 2 s window: at least 4 CAD hits at BW500 on at least 3 distinct ELRS
  channels, all with the same SF.
- The SF implies the packet rate: SF6 → 200 Hz, SF7 → 100 Hz, SF8 → 50 Hz,
  SF9 → 25 Hz (labelled "approx").

**ELRS 2.4**
- The same rule at BW812.5 across the 2.4 GHz channel plan.
- SF5 → 500 Hz, SF6 → 250 Hz, SF7 → 150 Hz, SF8 → 50 Hz (approx).

**FHSS RC (Crossfire / R9 / other)**
- Within a 2 s window: hot RSSI bins at no fewer than 6 distinct frequencies in
  the 868 or 915 band.
- Bins are short (only present for one sweep step at a time), repeat, and are
  not explained by an ELRS CAD hit.
- Confidence HEURISTIC.

**Synthetic IDs**: `ELRS900-SF7`, `ELRS24-SF6`, `FHSS915`. Two simultaneous
links with the same settings merge into one target; this limitation is shown
in the UI help.

## 6. UI

- Screen is 222×480 portrait. A status bar shows region, the LR1121 plan in
  progress, SD state, battery % and charge state.

| Button | Short press | Long press |
|---|---|---|
| BOOT (0) | back | — |
| 12 | up / previous | lock or unlock the selected target |
| 16 | down / next | open Settings |

- Touch works on every screen. If touch fails at startup, the buttons cover all
  navigation.

### Screens

**Targets (home)**
- Sorted by confidence, then RSSI.
- Each row has:
  - a type icon;
  - a label (serial / `ELRS 915 ~100Hz` / `!a1b2c3d4` / `TELLO-5F3A`);
  - frequency or channel;
  - an RSSI bar;
  - age.
- Colors by confidence: red DECODED drone/ELRS, amber HEURISTIC, grey LoRa nodes.
- Stale rows are dimmed.

**Hunt**
- Big vertical RSSI meter with the smoothed dBm value.
- Trend arrow (warmer / colder / steady) and peak hold with a reset button.
- A 30 s RSSI history graph.
- Remote ID detail panel: drone lat/lon/alt/speed/heading, pilot lat/lon,
  drone-to-pilot distance (haversine), operator ID, UA type.
- Hint line: "Hold to chest, turn slowly — strongest when you face it."
- **Audio** (MAX98357A) and **vibration** (GPIO45): pulse rate rises from
  1 Hz to 10 Hz and pitch from 400 Hz to 2 kHz as smoothed RSSI goes from
  −110 to −40 dBm.
  Each can be muted separately.

**Spectrum**
- Waterfall of RSSI sweep data for the selected band (915/868/433/2.4 GHz).
- CAD hits are overlaid with their SF/BW tag.

**Settings** (saved in NVS)
- Region FCC/EU.
- Enable/disable each detector.
- Alert mode off / banner / banner + sound + vibration.
- Volume, brightness, SD logging on/off.
- Show LoRa nodes yes/no.
- About/debug: LR1121 firmware version, radio error counters, task restarts,
  free heap.

### Alerts

| Event | Response |
|---|---|
| New DECODED drone or ELRS link | full-width red banner (tap → Hunt) + 2-tone chirp + double vibration |
| New HEURISTIC hit | amber notice, single short vibration |
| Meshtastic/LoRaWAN | list only, no alert |

- No repeat alerts for a target until it has aged out and come back.

## 7. Error handling

**Bring-up**
1. RT9080 EN high.
2. I²C, then PMU.
3. SPI, then display.
4. Touch.
5. LR1121.
6. SD.
7. Wi-Fi and BLE.

Each step reports OK or FAIL on a boot screen and over serial. Failures are
handled as follows:

| Failure | Effect |
|---|---|
| LR1121 | LoRa/RC detectors disabled, status bar shows `LORA ✕ <code>`, the rest keeps working |
| SD | logging off, `SD –` |
| Touch | buttons only |
| PMU | battery shown as `--` |

**Run time**
- Every RadioLib return code is checked. If a plan gets 5 consecutive errors it
  is suspended for 30 s and retried, and the error counter goes up.
- Radio tasks have task watchdogs. A stall restarts the task and increments a
  counter shown in About/debug.
- Queues are bounded. If one overflows, observations are dropped and counted;
  the radio tasks never block on the UI.
- All parsers take `(const uint8_t*, size_t)` and never read past the end of
  the buffer. Malformed input returns an error enum, never an assert.

## 8. Logging

- **SD file**: `/neth3t/S<NNN>.csv`, a new file per session. Columns:
  `t_ms,iso_time,type,conf,id,freq_khz,rssi,snr,sf,bw_khz,lat,lon,alt_m,op_lat,op_lon,vendor,extra`.
- Writes are buffered in PSRAM and flushed every 2 s or 4 KB.
- The same rows are mirrored to USB serial with a `LOG,` prefix.
- **Serial commands**: `time <unix>`, `region fcc|eu`, `dump` (prints the target
  table).

## 9. Testing

**Host tests** (`pio test -e native`, Unity)
- `odid`:
  - Basic ID, Location, System, Operator ID and Message Pack decoding from
    hand-assembled frames built to the spec, plus encoder round-trips.
  - Truncated and oversized inputs.
- `wifi_frames`:
  - Synthetic beacon with an ODID vendor IE.
  - Synthetic NAN service discovery frame.
  - Non-matching IEs ignored; truncated IE lengths.
- `ble_adv`: legacy service data with UUID 0xFFFA; wrong UUID or app code ignored.
- `drone_vendors`: OUI and SSID matches; no false match on common routers.
- `lora_headers`: Meshtastic header field extraction; LoRaWAN data-up and
  join-request.
- `rc_fingerprint`:
  - Simulated ELRS hit sequences, each at its own SF → correct class and rate.
  - Too few distinct channels → no class.
  - FHSS bin pattern detected; a static carrier gives no FHSS.
- `target_db`: merge, EMA, trend thresholds, peak, ageing, eviction order,
  lock pinning.

**Build**: `pio run -e t-display-s3-pro-lr1121` with zero errors.

**Field checklist** (`docs/FIELD_TEST.md`)
- An OpenDroneID transmitter app on Android (BLE legacy and Wi-Fi beacon).
- An ELRS TX module or radio at the lowest power, in 900 and 2.4 modes.
- A Meshtastic node on the default channel.
- A LoRaWAN device or a known nearby gateway area.
- A walk test of Hunt-mode trend, peak hold, audio and vibration.

## 10. Repository changes

- Replace the README with documentation for the new device:
  - purpose and legal notice (passive reception only; check local law on RF
    monitoring and logging);
  - hardware, build and flash instructions;
  - usage.
- Keep LICENSE.
- Add `platformio.ini` with envs `t-display-s3-pro-lr1121` and `native`.
- Remove all references to the old D-duino, CC1101, NRF24, PN5180 and IR
  hardware.
