# NETH3T field test checklist

Run these on the real device after flashing, in order. Keep the serial monitor
open alongside (`pio device monitor -b 115200`). Serial commands such as `dump`
and `stats` are typed into that monitor.

You need: an Android phone with the OpenDroneID "Transmitter" app, ideally an
ELRS transmitter (lowest power), and optionally a Meshtastic node, a LoRaWAN
device and a microSD card.

Note: on the finished firmware the Targets screen replaces the bring-up
touch/button test, so for step 1 check the boot screen and serial output
(buttons and touch are exercised in steps 4–5).

## 1. Bring-up

Startup shows the Brushbeater logo and "NETH3T" for about 2 s with the boot results underneath.

| Step | Expected |
|---|---|
| Power on | Boot screen lists `PMU OK`, `DISPLAY OK`, `TOUCH OK`; serial prints matching `BOOT … OK` lines |
| Boot screen | Also lists `RTC`, `LORA` and `SD` (SD FAIL is expected with no card) |
| After boot | Targets screen appears with the status bar (region, LoRa plan, SD, battery) |

## 2. Remote ID

| Step | Expected |
|---|---|
| Android phone running the OpenDroneID "Transmitter" app, BLE legacy mode | Serial `dump` shows a `REMOTEID_BLE` target whose id is the app's serial, with its location |
| Switch the app to BLE Long Range (coded PHY) if the phone supports it | Same target keeps updating |
| Switch the app to Wi-Fi Beacon | The same target (same serial) updates, now reported as `REMOTEID_WIFI` |
| Phone hotspot renamed `TELLO-TEST` | A `DRONE_WIFI` target labelled `TELLO-TEST`, vendor `Ryze/DJI Tello` |

## 3. LoRa / RC links

| Step | Expected |
|---|---|
| Boot | `BOOT LORA OK`; Settings → About shows an LR11x0 version string |
| ELRS TX module or radio at its lowest power, 915/868 band, packet rate 100 Hz | Within ~30 s an `ELRS900-SF7` target labelled `ELRS 915 ~100Hz` (or `ELRS 868 …`) |
| Same TX in 2.4 GHz LoRa mode (not FLRC), 250 Hz | `ELRS24-SF6` labelled `ELRS 2.4G ~250Hz` |
| Meshtastic node on the default LongFast channel sending a message | `MESHTASTIC` target `!xxxxxxxx` matching the node ID |
| LoRaWAN device (or near a busy gateway) | `LORAWAN` target `DA:xxxxxxxx` |
| Serial `stats` | `cad_hits` and `rx_pkts` counters rising while the above run |

## 4. Targets list and Hunt

| Step | Expected |
|---|---|
| With the phone Remote ID app running | Targets list shows a red `D` row with the serial; rows sorted decoded-first |
| Tap the row (or select it and long-press top-right) | Hunt screen opens: big dBm number, meter, trend arrow, 30 s graph |
| Walk ~20 m away | Arrow turns blue (colder) within about 3 s; number falls |
| Walk back | Arrow turns green (warmer); peak line holds the best value |
| Tap RESET PEAK | Peak line drops to the current reading |
| Detail panel | Shows the app's drone and pilot coordinates; "no fix" when the app sends 0,0 |
| BOOT button | Returns to Targets and releases the radio lock |

## 5. Spectrum and Settings

| Step | Expected |
|---|---|
| On Targets, swipe left | Spectrum screen; waterfall scrolls as sweeps arrive; up/down (or tap) cycles 915/868 → 433 → 2.4 GHz |
| Key an ELRS TX | Bright streaks across the 915 (or 868) waterfall; `SFn` ticks appear under it |
| Long-press bottom-right → Settings; change Region to EU | Band 0 axis now reads 863.0 … 870.0; back to FCC reads 902.0 … 928.0 |
| Toggle a few settings, power-cycle | Settings are unchanged after reboot |
| About rows | LR11x0 version, CAD/RX counters, `restarts 0` |

## 6. Alerts and hunt feedback

| Step | Expected |
|---|---|
| Start the phone Remote ID transmitter (Settings → Alerts `full`) | One red banner, a two-tone chirp and a double vibration; no repeat while the target stays in the list |
| Tap the banner | Hunt opens on that target |
| Phone hotspot named `TELLO-TEST` | Amber "Possible drone" banner and one short vibration |
| Meshtastic node appears | Listed only; no banner, no sound |
| On Hunt, walk toward the transmitter | Beeps get faster and higher in pitch; vibration pulses speed up |
| Settings → Sound off / Vibration off | Each feedback channel mutes independently |
| Stop the transmitter while on Hunt | Beeps stop within ~3 s |

## 7. Logging and serial commands

| Step | Expected |
|---|---|
| Boot with a FAT32 microSD card inserted | `BOOT SD OK`, serial `LOG file /neth3t/S000.csv` (next free number on later boots); status bar `SD+` |
| Run 5 minutes near any target | File has the CSV header and one row per observation; serial shows the same rows prefixed `LOG,` |
| Serial: `time <unix seconds>` (e.g. paste the output of `date +%s`) | `OK time set (RTC updated)`; new rows carry `iso_time`; still correct after a power-cycle |
| Serial: `dump` / `stats` / `region eu` | Target table / counters / region switch confirmed with `OK` |
| Boot without a card | `BOOT SD FAIL`, `SD-`; everything else works and rows still go to serial |

## 8. Navigation

| Step | Expected |
|---|---|
| Tap each bottom tab | Switches between Targets, Spectrum and Settings; the current tab is highlighted |
| Open Hunt on any target, then press the Home key (printed circle under the screen) | Returns to Targets; the radios stop focusing on that target |
| On Hunt, tap **Back** (top-right) | Returns to Targets |
