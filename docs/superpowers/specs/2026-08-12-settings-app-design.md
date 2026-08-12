# Settings App Design

## Goal

Port the `app_setup` Settings app from the sibling reference project
`M5StopWatch-UserDemo` into this project as a fifth app: brightness, volume,
button (SFX/vibration) config, set time, set date, and a hidden "About"
easter egg (tap the firmware version label 10 times).

## Why this is a port, not a fresh design

This project's multi-app framework was itself bootstrapped from
`M5StopWatch-UserDemo`'s architecture at the start of this whole effort, so
`app_setup` targets the same mooncake/LVGL-v9/`uitk::lvgl_cpp` stack this
project already uses — verified compatible piece by piece before writing
this spec:

- HAL: `getBackLightBrightness`/`setBackLightBrightness`,
  `getSpeakerVolume`/`setSpeakerVolume`, `getButtonConfig`/`setButtonConfig`
  (`ButtonConfig{sfxEnabled, vibrateEnabled}`), `getTimeHms`/`setTimeHms`,
  `getDateYmd`/`setDateYmd` (`DateYmd` with `isValid()`/`daysInMonth()`) all
  already exist in `main/hal/hal.h` with identical signatures.
- Widgets: `Roller`, `Slider`, `Switch` all already exist in
  `components/smooth_ui_toolkit/src/lvgl/lvgl_cpp/` with the exact methods
  the reference code calls (`setOptions`/`setSelected`/`setVisibleRowCount`,
  `setRange`/`setValue`, `onValueChanged()`).
- `fmt::format` is available (bundled inside `mooncake_log`, already a
  dependency of `main`, public include at `components/mooncake_log/src/`).
- `common::FirmwareVersion` already exists in `main/apps/common/common.h`.
- `Random::getInstance()` already exists in
  `components/smooth_ui_toolkit/src/tools/random/random.hpp`.
- The reference `icon_setup.c` (200x200 RGB565) already has a pure-black
  corner pixel (`0x0000`) — no background-color fix needed, unlike the other
  three launcher icons earlier in this project.
- The reference `CommissionerMedium108.c` font (74KB) can be copied as-is.

Given this, the plan is a close port: copy the reference files, adjust
namespaces/includes only where this project's layout differs, and register
the new app the same way Moments/Stopwatch/Todo/Pendulum were registered.

## Scope

Port all six pieces from the reference `app_setup`, unchanged in behavior:

1. **Menu** (`view::SelectMenuPage`) — scrollable list of sections
   (Device / Time & Date / Firmware), each with big rounded buttons.
2. **Brightness** — percentage slider (10-100, step 1) writing
   `setBackLightBrightness`.
3. **Volume** — percentage slider (0-100, step 5) writing `setSpeakerVolume`.
4. **Button** — two switches (SFX, Vibration) writing `setButtonConfig`.
5. **Set Time** — three infinite rollers (hour/minute/second) writing
   `setTimeHms`. This is the piece that directly benefits the Pendulum
   clock app added earlier — the clock reads `getTimeHms()` but until now
   nothing in this project could ever set it.
6. **Set Date** — year/month rollers then a day roller (two-stage, matching
   the reference's "Next" -> "OK" flow), writing `setDateYmd`.
7. **About easter egg** — tapping the firmware-version menu item 10 times
   opens a fake Windows-BSOD-style progress screen (pure decoration, no
   real effect on the device).

Kept as-is from the reference: the dark theme (black panels, white text,
green `0x4AD78C` OK-button accent). This project's Stopwatch app is already
dark-themed, so this doesn't clash with the project's overall look — Settings
reads as a "system" surface, distinct from the light Todo/Pendulum apps, same
as it does in the reference project.

## File Layout

Mirrors the reference project's structure under a new
`main/apps/app_settings/` (named to match this project's `Settings` app
name convention rather than the reference's internal `app_setup` directory
name — the class itself is still `AppSettings` to fit this project's
`AppMoments`/`AppStopwatch`/`AppTodo`/`AppPendulum` naming pattern):

- `app_settings.h` / `.cpp` — lifecycle, menu-section wiring, worker
  dispatch (ported from `app_setup.h/.cpp`).
- `view/view.h` / `.cpp` — `SelectMenuPage` (ported from `view/view.h/.cpp`,
  unchanged).
- `workers/workers.h` — `WorkerBase` + the six worker class declarations
  (ported from `workers/workers.h`, unchanged).
- `workers/device.cpp` — `PercentageAdjustView`, `BrightnessWorker`,
  `VolumeWorker`, `ButtonWorker::ButtonConfigView`, `ButtonWorker` (ported
  from `workers/device.cpp`, unchanged).
- `workers/datetime.cpp` — `SetTimeWorker::TimeAdjustView`, `SetTimeWorker`,
  `SetDateWorker::DateAdjustView`, `SetDateWorker` (ported from
  `workers/datetime.cpp`, unchanged).
- `workers/about.cpp` — `AboutWorker::AboutView`, `AboutWorker` (ported from
  `workers/about.cpp`, unchanged).

New assets:
- `main/assets/fonts/CommissionerMedium108.c` — copied verbatim from the
  reference project.
- `main/assets/images/icon_settings_app.c` — copied verbatim from the
  reference project's `icon_setup.c` (renamed to match this project's
  `icon_<name>_app` convention).

Registration: `LV_FONT_DECLARE(CommissionerMedium108)` and
`LV_IMG_DECLARE(icon_settings_app)` added to `main/assets/assets.h`;
`#include "app_settings/app_settings.h"` added to `main/apps/apps.h`;
`GetMooncake().installApp(std::make_unique<AppSettings>());` added to
`main/main.cpp`.

## Non-goals

- No new settings beyond what the reference has — no Wi-Fi config, no
  factory reset, nothing this project doesn't already have a HAL hook for.
- No attempt to reskin Settings to match Todo/Pendulum's light theme —
  keeping the reference's dark theme as designed (see Scope above).
- Not wiring Settings into the Pendulum clock or any other app — it's a
  standalone app, same as the other four.
