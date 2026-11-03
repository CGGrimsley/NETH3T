![N3 Logo](https://i.imgur.com/xraBuH7.png)

# NETH3T: handheld anti-drone and LoRa detector

NETH3T turns a **LilyGO T-Display-S3-Pro with the MVSRLora back plate (LR1121)**
into a pocket receiver that finds drones, RC control links and LoRa devices. It
then guides you to the one you pick, using a live signal-strength meter, beeps
and vibration.

It only listens. It never transmits against anything.

> **Legal notice.** NETH3T is a receive-only monitoring tool. Laws on radio
> monitoring, on logging what you receive, and on using Remote ID data
> (drone or pilot locations, operator IDs) differ from country to country.
> Some countries restrict all three. Check your local law before you use it.
> Never jam, spoof or otherwise interfere with aircraft or their control
> links; in most places that is a serious crime and dangerous. This firmware
> has no transmit path for any of those things.

## What it detects

| Source | How | Confidence |
|---|---|---|
| **Remote ID drones** (ASTM F3411 / EU ASD-STAN): DJI and most modern drones | Wi-Fi beacon and Wi-Fi NAN frames, BLE 4 legacy and BLE 5 Long Range adverts; decodes serial, drone position/altitude/speed, **pilot position**, operator ID | Decoded |
| **ExpressLRS** 900 MHz and 2.4 GHz (LoRa modes) | Channel-activity detection on the ELRS channel plans; the hop pattern plus the spreading factor gives band and approximate packet rate | Fingerprint |
| **Older DJI DroneID beacons** | DJI vendor element in Wi-Fi beacons | Fingerprint |
| **Drone-maker Wi-Fi** (DJI, Parrot, Skydio, Tello, Autel, …) | MAC vendor prefixes (IEEE registry) and SSID patterns | Heuristic |
| **Other frequency-hopping RC links** (Crossfire, R9, …) | Bursts hopping across the 868/915 band in the RSSI sweep | Heuristic |
| **Meshtastic** nodes | LoRa receive on the default presets; node ID from the unencrypted header | Decoded |
| **LoRaWAN** devices | LoRa receive on the default uplink channels; DevAddr / DevEUI from the header | Decoded |

### What it does not do

- It does not decrypt Meshtastic or LoRaWAN payloads, or decode ELRS packets.
  ELRS needs the pilot's binding phrase.
- It cannot demodulate DJI OcuSync / O3 / O4 video links; this hardware can't
  handle that kind of wideband signal. DJI aircraft are caught by their
  Remote ID broadcasts instead.
- ELRS FLRC modes (2.4 GHz F500/F1000) are invisible to the LR1121.
- It does not show distance or bearing to you: the board has no GPS or compass.
  For Remote ID drones it does show the **drone-to-pilot** distance.
- Two ELRS links with identical settings show as one target.

## Hardware

- LilyGO **T-Display-S3-Pro** (ESP32-S3, 16 MB flash, 8 MB PSRAM, 2.33" 222×480
  touch screen) with the **MVSRLora** back plate (LR1121, speaker, vibration
  motor, RTC). See
  [T-Display-S3-Pro-MVSRLora](https://github.com/Xinyuan-LilyGO/T-Display-S3-Pro-MVSRLora).
- Antenna(s) on the LR1121 connector, sized for the bands you care about.
- Optional: a FAT32 microSD card for CSV logs.

All pins are in [`include/board_pins.h`](include/board_pins.h).

## Build and flash

Install [PlatformIO](https://platformio.org/install/cli), then from the repository root:

```bash
pio run -e t-display-s3-pro-lr1121 -t upload
```

If no serial port appears, hold **BOOT** (bottom-left button), plug in USB,
release, and upload again. To watch the log:

```bash
pio device monitor -b 115200
```

## Using it

**Screens**
- **Targets** (home): the live list, decoded drones first.
  - Red = decoded drone or ELRS link; amber = heuristic; grey = LoRa node.
  - Faded = not heard for 20 s; a row disappears after 60 s.
- **Hunt**:
  - big smoothed RSSI meter;
  - **warmer / colder** arrow (change over the last 3 s);
  - peak hold and a 30 s history graph;
  - beeps and vibration that speed up as you get closer;
  - for Remote ID: drone and pilot coordinates.
- **Spectrum**: waterfall of the 915/868, 433 and 2.4 GHz sweeps, with
  activity hits marked by spreading factor.
- **Settings**: region (FCC/EU), each detector on/off, alerts, sound,
  vibration, volume, brightness, SD logging, and About/debug counters.

**Hunting tip:** the antenna picks up equally in all directions. Hold the
device against your chest and turn slowly. Your body blocks signals from
behind you, so the reading is strongest when you face the transmitter.

| Control | Short press | Long press |
|---|---|---|
| BOOT (bottom-left) | Back / leave Hunt | — |
| Top-right | Up | Lock selected target (Hunt) / change setting |
| Bottom-right | Down | Settings |
| Touch | Tap a row to hunt it; tap the alert banner; bottom tabs switch **Targets / Spectrum / Settings**; **Back** (top-right) leaves Hunt | |
| Home key (printed circle under the screen) | Back to Targets from anywhere; releases a Hunt lock | |

**Alerts**

| Event | Response |
|---|---|
| New Remote ID drone or ELRS link | Red banner, chirp, double vibration |
| Heuristic hit | Amber banner, short vibration |
| LoRa nodes | Listed only |

## Logs and serial commands

Every observation is printed to USB serial as `LOG,<csv row>`. With a card
inserted, it is also appended to `/neth3t/SNNN.csv`, one file per power-on.
The columns are:

```
t_ms,iso_time,type,conf,id,freq_khz,rssi,snr,sf,bw_khz,lat,lon,alt_m,op_lat,op_lon,vendor,extra
```

Serial commands (115200 baud, newline-terminated):

| Command | Effect |
|---|---|
| `time <unix seconds>` | Set the clock (kept by the on-board RTC); rows then carry `iso_time` |
| `region fcc` / `region eu` | Switch channel plans |
| `dump` | Print the target table |
| `stats` | Radio counters, dropped observations, restarts, free memory |
| `sweep` | Latest raw RSSI sweep of each band, with its noise floor |

## Development

Parsers, classifiers, the target table, alert policy and CSV formatting are
plain C++ with no hardware dependencies, and have host unit tests:

```bash
pio test -e native
```

- Field checks for the hardware parts: [`docs/FIELD_TEST.md`](docs/FIELD_TEST.md).
- Design: [`docs/superpowers/specs/2026-11-03-neth3t-sigint-design.md`](docs/superpowers/specs/2026-11-03-neth3t-sigint-design.md).

## License

See [LICENSE](LICENSE).
