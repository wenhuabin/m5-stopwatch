# M5 StopWatch Firmware

ESP-IDF multi-app firmware for the M5Stack StopWatch, built on the
`mooncake` app framework. Apps live under `main/apps/`.

## Apps

- **Moments** — a 5-photo slideshow with phone-browser upload (long-press
  to start a Wi-Fi hotspot + upload page, tap left/right to switch
  photos).
- **Stopwatch** — start/pause/reset with lap recording and best/worst lap
  tracking. Ported from
  [VolosR/M5Stopwatch](https://github.com/VolosR/M5Stopwatch/tree/main/VolosStopwatch)
  (functionality and layout referenced; rebuilt on this project's
  mooncake/LVGL v9 stack rather than the original's SquareLine/LVGL v8
  Arduino sketch).

## Build

### Fetch Dependencies

```bash
python3 ./fetch_repos.py
```

### Tool Chains

[ESP-IDF v5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/index.html)

### Build

```bash
idf.py build
```

### Flash

```bash
idf.py flash
```
