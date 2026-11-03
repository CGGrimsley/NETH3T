# NETH3T field test checklist

Run these on the real device after flashing. Open the serial monitor
(`pio device monitor`, 115200 baud) alongside.

## 1. Bring-up

| Step | Expected |
|---|---|
| Power on | Boot screen lists `PMU OK`, `DISPLAY OK`, `TOUCH OK`; serial prints matching `BOOT … OK` lines |
| Touch the screen | A yellow dot appears under your finger |
| Press each button | Serial prints `BTN 1` (BOOT), `BTN 2` (top right), `BTN 3` (bottom right); hold top/bottom right 1 s → `BTN 4` / `BTN 5` |
