# M5 StopWatch: Multi-App Framework with Moments as the First App

Date: 2026-08-10
Status: Approved for planning

## Background

`m5-stopwatch` currently contains a single MicroPython/UIFlow2 script,
`Moments.py`: a five-photo slideshow that runs a Wi-Fi + HTTP upload server on
the watch so a phone browser can push new photos.

The sibling project `M5StopWatch-UserDemo` (same physical hardware — M5Stack
StopWatch, ESP32-S3, 466x466 round touch display, two physical side buttons
BtnA/BtnB) is a full ESP-IDF C++ firmware built on the `mooncake` app
framework + LVGL. It already implements the pattern we want: a launcher that
shows an icon grid, and any number of installed "apps," each a C++ class with
a uniform lifecycle (`onCreate` / `onOpen` / `onRunning` / `onClose`),
switched via `GetMooncake().installApp(...)`.

Goal: replace the single-script approach with that multi-app architecture.
Moments becomes the first installed app, ported from Python to C++, keeping
its user-visible behavior (5-photo full-screen slideshow, tap left/right to
switch, phone-browser upload) but adopting the demo's conventions for app
lifecycle, storage, and Wi-Fi/upload UX (modeled on how the demo's Badge app
handles photo upload).

## Scope decisions (confirmed)

- **Tech stack**: migrate fully to ESP-IDF C++ / mooncake, matching
  `M5StopWatch-UserDemo`. `Moments.py` and `memory.m5f2` are deleted — no
  MicroPython path is kept.
- **Bundled demo apps**: not carried over. Only the framework (mooncake,
  launcher, HAL, `app_template`, shared `apps/common/` utilities) is kept.
  `app_watch_face`, `app_stopwatch`, `app_badge`, `app_imu`, `app_fft`,
  `app_lucky_wheel`, `app_alarm_clock`, `app_setup` and their dedicated
  assets/HAL glue are removed. Moments is the only real app after this
  change.
- **Upload trigger**: "edit mode" pattern, same as the demo's Badge app —
  Wi-Fi/HTTP server is off by default; a long-press on the photo view shows a
  confirm dialog, confirming starts the AP + upload server; finishing (or
  timing out) tears it down and returns to the slideshow. This differs from
  the original Python version's always-on server, traded for consistency
  with the rest of the codebase and lower power draw.
- **Network mode**: AP-only hotspot (`M5StopWatch-XXXX` SSID convention,
  same as the demo's `config_ap`), no STA-first-then-AP-fallback. Simpler,
  and consistent with how the demo already does it.
- **Build verification**: set up ESP-IDF v5.5.4 locally, run
  `fetch_repos.py`, and run `idf.py build` to confirm the pruned + extended
  project compiles. No physical hardware is available in this environment,
  so flashing/on-device testing is out of scope here.
- **Version control**: `m5-stopwatch` is initialized as a fresh git repo;
  this spec and subsequent implementation are committed incrementally.

## Architecture

### Bootstrap

Copy `M5StopWatch-UserDemo` into `m5-stopwatch` as the project baseline
(`CMakeLists.txt`, `partitions.csv`, `sdkconfig.defaults`, `repos.json`,
`fetch_repos.py`, `.github/`, `.gitignore`, `main/`). Then prune to the
scope above.

**Kept:**
- `components/mooncake`, `components/mooncake_log` (fetched via
  `fetch_repos.py`, not vendored)
- `main/hal/` core: `hal.cpp/.h`, `hal_display.cpp`, `hal_button.cpp`,
  `hal_ioe.cpp`, `hal_pmic.cpp`, `hal_imu.cpp` (sensor driver, not the demo
  app), `hal_rtc.cpp`, `hal_fs.cpp`, `hal_audio.cpp`, and their `drivers/` /
  `utils/` (`wear_levelling`, `button/Button_Class`, `settings`) —
  everything needed to boot the display, touch, buttons, and mounted
  storage.
- `main/apps/app_launcher/`, `main/apps/app_template/`
- `main/apps/common/` shared utilities used by the launcher/status bar
  (`key_manager`, `status_bar`, `loading_page`, `arc_top_clock`)
- Fonts and any image assets still referenced by launcher/common code after
  pruning (verified by grep during implementation, not assumed up front)

**Removed:**
- `app_watch_face`, `app_stopwatch`, `app_badge`, `app_imu` (the app, HAL
  IMU driver stays), `app_fft`, `app_lucky_wheel`, `app_alarm_clock`,
  `app_setup`
- `hal_badge.cpp`, `hal/utils/config_ap/` (badge-only network module —
  Moments gets its own, see below), `hal_alarm.cpp` if nothing else uses it
- Assets exclusively used by removed apps (watch-face big-number/classic
  images, lucky-wheel images, badge/alarm icons, badge config-ap HTML)

**Added:**
- `main/apps/app_moments/` — the new app (see below)
- `main/assets/images/icon_moments.c` (+ `LV_IMG_DECLARE` in `assets.h`) — a
  simple placeholder icon, 200x200 RGB565, same format as existing icons

`main/main.cpp` installs only `AppLauncher` and `AppMoments`.

### AppMoments lifecycle

`main/apps/app_moments/app_moments.{h,cpp}`, modeled directly on
`AppBadge`/`hal_badge.cpp`:

```cpp
class AppMoments : public mooncake::AppAbility {
public:
    AppMoments();
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;
private:
    std::unique_ptr<input::KeyManager> _key_manager;
    std::unique_ptr<view::MomentsView> _view;
};
```

- `setAppInfo().name = "Moments"`, `setAppInfo().icon = (void*)&icon_moments`.
- `onOpen()`: ensure `/spiflash/moments` exists, load + validate existing
  photos (drop anything not an exact-dimension BMP, same rule the Python
  version used), show the first photo full-screen or an empty-state hint
  ("No photos yet — hold to upload").
- `onRunning()`:
  - `KeyManager::update()` → `GoHome` (BtnA+BtnB held) calls `close()`.
  - Touch tap on left half → previous photo; right half → next photo
    (unchanged from the Python version's UX). Only active when not
    currently in upload mode.
  - Long-press anywhere on the photo view → show confirm dialog ("Upload
    new photos?"); confirming enters upload mode (see below); cancelling
    dismisses.
- `onClose()`: if upload mode is active, stop AP/HTTP server first; release
  LVGL image objects.

### Upload mode (Wi-Fi AP + HTTP server)

New module `main/apps/app_moments/net/upload_server.{h,cpp}`, written in the
same style as the demo's `hal/utils/config_ap/config_ap.cpp` (`esp_wifi`
AP-only setup + `esp_http_server`), but with the batch-upload semantics the
Python version had (`config_ap` itself is single-image-per-slot with no
batch concept, so this is a new module rather than a literal reuse of
`badge::config_ap`).

Entry point, called from `AppMoments` similar to
`Hal::startBadgeEditModeViaAp`:

```cpp
namespace moments::net {
void run_upload_mode(const std::function<void(std::string_view)>& onLog,
                      const std::function<void()>& onPhotosChanged);
}
```

Behavior, ported from the Python implementation:
- Starts a dedicated AP (`M5StopWatch-XXXX-Moments` SSID, same MAC-suffix
  convention as `config_ap::make_ap_ssid`), starts `esp_http_server` on port
  80.
- `GET /` serves an embedded HTML page (`main/apps/app_moments/assets/
  moments_upload.html`, `EMBED_TXTFILES` like the Badge config page) — the
  original upload page's JS is kept essentially as-is: picks 1-5 images,
  draws each into a 466x466 `<canvas>` (stretched, matching the original
  behavior exactly), encodes as an uncompressed BMP client-side, uploads
  sequentially.
- `POST /upload?batch=<token>&reset=1&last=1`: streams the body straight to
  a temp file, validates BMP signature + exact 466x466 dimensions, renames
  into `/spiflash/moments/moment_<batch>_<index>.bmp` on success. `reset=1`
  (first file in a batch) deletes the previous batch first. `last=1` (final
  file) signals the app to reload the slideshow.
- `GET /status`: JSON `{photos, max_photos, max_file_bytes}`, same shape as
  before.
- A "done" affordance (explicit close button on the page, or an idle
  timeout) tears the AP/server down and returns control to `AppMoments`,
  which reloads photos and resumes the slideshow.

Rationale for keeping the client-side BMP pre-resize step (rather than
switching to Badge's "upload raw JPG, let LVGL decode+scale" approach): the
Python version's code comments make clear this was a deliberate ESP32-S3
performance fix — decoding a multi-megapixel phone JPEG on-device blocked
startup and every slide switch. Pre-resizing to an exact, uncompressed match
for the display means the watch only ever blits raw pixels, no decode. This
matters more for Moments (full-screen photos, frequent switching) than it
did for Badge (one small avatar image).

### Storage layout

`/spiflash/moments/` on the existing `wear_levelling`-backed FAT partition
(same partition Badge used, `storage` in `partitions.csv`, mounted at
`/spiflash` by `Hal::fs_init()`).

- Up to 5 files per batch: `moment_<batch_id>_<index>.bmp`
- `upload.tmp` — scratch file during a single file's upload, removed on
  failure
- A new batch fully replaces the previous one (delete-then-write, matching
  the Python `_delete_all_photos()` behavior)

### Error handling

Ported 1:1 from the Python HTTP handler, expressed as ESP-IDF
`esp_http_server` response codes + JSON bodies:

| Condition | Response |
|---|---|
| Unsupported content-type | 415, `{"error":"..."}` |
| Empty body / bad content-length | 400 |
| File over 2 MiB | 413 |
| More than 5 photos in a batch | 409 |
| Wrong BMP dimensions / bad signature | 400 |
| Not enough free space on `/spiflash` | 507 |
| Upload interrupted (socket closed mid-body) | temp file discarded, no
  partial file ever becomes visible |

On `onOpen()`, any file in `/spiflash/moments` that isn't a valid
exact-dimension BMP is dropped (covers stale/legacy files), same as
`_load_existing_photos()` did.

## Build verification plan

1. Install ESP-IDF v5.5.4 locally (idf_tools.py install + export), matching
   the toolchain the demo README specifies.
2. Run `python3 ./fetch_repos.py` to pull `mooncake`, `mooncake_log`,
   `smooth_ui_toolkit`, `M5GFX`, `ArduinoJson`, `lvgl`, `M5IOE1`, `M5PM1`,
   `BMI270_BMM150_Sensor` into `components/` (only the ones still referenced
   after pruning need to stay wired into the build; unused ones can be
   dropped from `repos.json`/`idf_component.yml` if nothing in the pruned
   tree needs them).
3. `idf.py build` — confirms the pruned framework + new `app_moments` module
   compiles cleanly. No flashing (no hardware available in this
   environment); build success is the acceptance bar here.

## Out of scope

- Any app beyond Moments (framework is built to support more later, none
  are implemented now).
- On-device/hardware testing (flashing, real Wi-Fi upload from a phone).
- STA Wi-Fi mode / reusing a pre-configured network.
- Keeping the old MicroPython script path alive in any form.
