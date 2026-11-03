# NETH3T field test checklist

Run these on the real device after flashing. Open the serial monitor
(`pio device monitor`, 115200 baud) alongside.

## 1. Bring-up

| Step | Expected |
|---|---|
| Power on | Boot screen lists `PMU OK`, `DISPLAY OK`, `TOUCH OK`; serial prints matching `BOOT … OK` lines |
| Touch the screen | A yellow dot appears under your finger |
| Press each button | Serial prints `BTN 1` (BOOT), `BTN 2` (top right), `BTN 3` (bottom right); hold top/bottom right 1 s → `BTN 4` / `BTN 5` |

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
