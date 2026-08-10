# M5 StopWatch Firmware

ESP-IDF multi-app firmware for the M5Stack StopWatch, built on the
`mooncake` app framework. Apps live under `main/apps/`; `Moments` (a
5-photo slideshow with phone-browser upload) is the first installed app
beyond the launcher.

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
