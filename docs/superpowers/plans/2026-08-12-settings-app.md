# Settings App Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the `app_setup` Settings app from the sibling reference project
`/Users/wenhuabin/Project/M5StopWatch-UserDemo` into this project as
`app_settings` — brightness, volume, button config, set time, set date, and
a hidden About easter egg.

**Architecture:** Six source files ported from the reference, five of them
**verbatim** (`view/view.h`, `view/view.cpp`, `workers/workers.h`,
`workers/device.cpp`, `workers/datetime.cpp`, `workers/about.cpp` — none of
them reference the app's own class/directory name internally, confirmed by
inspection), plus `app_settings.h`/`app_settings.cpp` which are the
reference's `app_setup.h`/`app_setup.cpp` with `AppSetup` renamed to
`AppSettings` and the icon symbol renamed to match this project's
`icon_<name>_app` convention. Two assets copied from the reference
(`CommissionerMedium108.c` font verbatim, `icon_setup.c` renamed to
`icon_settings_app.c`).

## Global Constraints

- No automated test framework for this embedded/LVGL codebase — verification
  is build-clean + flash-and-manually-check on the physical device at
  `/dev/cu.usbmodem1101`, same as every prior app in this project.
- Source ESP-IDF before any `idf.py` command: `. ~/esp/esp-idf/export.sh`.
- New `.cpp` files need `idf.py reconfigure` before the next build.
- Check `lsof /dev/cu.usbmodem1101` and kill any leftover listener before
  flashing.
- Reference source lives at
  `/Users/wenhuabin/Project/M5StopWatch-UserDemo/main/apps/app_setup/` and
  `/Users/wenhuabin/Project/M5StopWatch-UserDemo/main/assets/`. Every `cp`
  command below reads from there.
- Commit after each task.

---

### Task 1: Assets + menu skeleton (no worker wiring yet)

**Files:**
- Create: `main/assets/fonts/CommissionerMedium108.c` (copied verbatim)
- Create: `main/assets/images/icon_settings_app.c` (copied + renamed from `icon_setup.c`)
- Modify: `main/assets/assets.h`
- Create: `main/apps/app_settings/view/view.h` (copied verbatim)
- Create: `main/apps/app_settings/view/view.cpp` (copied verbatim)
- Create: `main/apps/app_settings/workers/workers.h` (copied verbatim)
- Create: `main/apps/app_settings/app_settings.h`
- Create: `main/apps/app_settings/app_settings.cpp`
- Modify: `main/apps/apps.h`
- Modify: `main/main.cpp`

**Interfaces:**
- Produces: `class AppSettings : public mooncake::AppAbility`, registered in
  the launcher, opening to a scrollable dark menu (Device / Time & Date /
  Firmware sections) whose items are all no-ops for now — Tasks 2-4 replace
  each section's stub lambdas with real worker construction one at a time.

- [ ] **Step 1: Copy the font and icon assets**

```bash
cp /Users/wenhuabin/Project/M5StopWatch-UserDemo/main/assets/fonts/CommissionerMedium108.c \
   /Users/wenhuabin/Project/m5-stopwatch/main/assets/fonts/CommissionerMedium108.c

cp /Users/wenhuabin/Project/M5StopWatch-UserDemo/main/assets/images/icon_setup.c \
   /Users/wenhuabin/Project/m5-stopwatch/main/assets/images/icon_settings_app.c

sed -i '' 's/icon_setup/icon_settings_app/g; s/LV_ATTRIBUTE_IMAGE_ICON_SETUP/LV_ATTRIBUTE_IMAGE_ICON_SETTINGS_APP/g' \
   /Users/wenhuabin/Project/m5-stopwatch/main/assets/images/icon_settings_app.c
```

Verify the rename caught every occurrence (expect zero matches for the old names):

```bash
grep -c "icon_setup\|ICON_SETUP" /Users/wenhuabin/Project/m5-stopwatch/main/assets/images/icon_settings_app.c
```

- [ ] **Step 2: Declare the new assets** — add to `main/assets/assets.h`:

```cpp
LV_FONT_DECLARE(CommissionerMedium108);
```

(alongside the other `LV_FONT_DECLARE` lines)

```cpp
LV_IMG_DECLARE(icon_settings_app);
```

(alongside the other `LV_IMG_DECLARE(icon_*_app)` lines)

- [ ] **Step 3: Copy the menu view verbatim**

```bash
mkdir -p /Users/wenhuabin/Project/m5-stopwatch/main/apps/app_settings/view
mkdir -p /Users/wenhuabin/Project/m5-stopwatch/main/apps/app_settings/workers

cp /Users/wenhuabin/Project/M5StopWatch-UserDemo/main/apps/app_setup/view/view.h \
   /Users/wenhuabin/Project/m5-stopwatch/main/apps/app_settings/view/view.h

cp /Users/wenhuabin/Project/M5StopWatch-UserDemo/main/apps/app_setup/view/view.cpp \
   /Users/wenhuabin/Project/m5-stopwatch/main/apps/app_settings/view/view.cpp
```

Diff against a sibling app's view to confirm no `app_setup`-specific content
snuck in (expect no output — this file is self-contained):

```bash
grep -n "app_setup\|AppSetup" /Users/wenhuabin/Project/m5-stopwatch/main/apps/app_settings/view/view.h \
  /Users/wenhuabin/Project/m5-stopwatch/main/apps/app_settings/view/view.cpp
```

- [ ] **Step 4: Copy the worker declarations verbatim**

```bash
cp /Users/wenhuabin/Project/M5StopWatch-UserDemo/main/apps/app_setup/workers/workers.h \
   /Users/wenhuabin/Project/m5-stopwatch/main/apps/app_settings/workers/workers.h
```

(This declares all six worker classes — `BrightnessWorker`, `VolumeWorker`,
`ButtonWorker`, `SetTimeWorker`, `SetDateWorker`, `AboutWorker` — but only
`WorkerBase` has an inline definition. The other five are fully valid as
forward-declared member types for now; they don't need a `.cpp` definition
until something actually constructs one, which doesn't happen until Tasks
2-4.)

- [ ] **Step 5: Create `main/apps/app_settings/app_settings.h`**

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "view/view.h"
#include "workers/workers.h"
#include <apps/common/key_manager/key_manager.h>
#include <mooncake.h>
#include <cstdint>
#include <memory>

class AppSettings : public mooncake::AppAbility {
public:
    AppSettings();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::vector<view::SelectMenuPage::MenuSection> _menu_sections;
    std::unique_ptr<view::SelectMenuPage> _menu_page;
    std::unique_ptr<setup_workers::WorkerBase> _worker;
    std::unique_ptr<input::KeyManager> _key_manager;

    bool _destroy_menu    = false;
    bool _need_warm_reset = false;
    int _magic_count      = 0;
};
```

- [ ] **Step 6: Create `main/apps/app_settings/app_settings.cpp`** (Task 1
  version — all menu items are no-ops; Tasks 2-4 replace them one section at
  a time)

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_settings.h"
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <assets/assets.h>

using namespace mooncake;
using namespace view;
using namespace setup_workers;

AppSettings::AppSettings()
{
    setAppInfo().name = "Settings";
    setAppInfo().icon = (void*)&icon_settings_app;
}

void AppSettings::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppSettings::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();

    _destroy_menu    = false;
    _need_warm_reset = false;
    _magic_count     = 0;

    _menu_sections = {
        {
            "Device",
            {
                {"Brightness", [&]() {}},
                {"Volume", [&]() {}},
                {"Button", [&]() {}},
            },
        },
        {
            "Time & Date",
            {
                {"Set Time", [&]() {}},
                {"Set Date", [&]() {}},
            },
        },
        {
            "Firmware",
            {
                {fmt::format("Version: {}", common::FirmwareVersion), [&]() {}},
            },
        },
    };

    LvglLockGuard lock;

    _menu_page = std::make_unique<view::SelectMenuPage>(_menu_sections);
}

void AppSettings::onRunning()
{
    if (_key_manager && _key_manager->update() == input::KeyEvent::GoHome) {
        close();
        return;
    }

    LvglLockGuard lock;

    if (_menu_page) {
        _menu_page->update();
    }

    if (_destroy_menu) {
        _menu_page.reset();
        _destroy_menu = false;
    }

    if (_worker) {
        _worker->update();
        if (_worker->isDone()) {
            _worker.reset();
            _menu_page = std::make_unique<view::SelectMenuPage>(_menu_sections);
        }
    }
}

void AppSettings::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();

    LvglLockGuard lock;

    _menu_sections.clear();
    _menu_page.reset();
    _worker.reset();

    if (_need_warm_reset) {
        // GetHAL().requestWarmReboot(6);
    }
}
```

> Note: `fmt::format` and `common::FirmwareVersion` both need `<fmt/format.h>`
> and `apps/common/common.h` respectively. Neither reference `app_setup.cpp`
> nor this file includes them explicitly — the reference project compiles
> this way because `mooncake_log.h` transitively pulls in `fmt/format.h`,
> and `assets/assets.h` (or a header it includes) transitively pulls in
> `common.h` in that project. **If Step 7's build fails on either symbol**,
> add `#include <fmt/format.h>` and/or `#include <apps/common/common.h>` to
> this file — check which one is actually missing from the compiler error
> before adding both speculatively.

- [ ] **Step 7: Register the app** — add to `main/apps/apps.h`:

```cpp
#include "app_settings/app_settings.h"
```

And to `main/main.cpp`, in the "Install apps" section:

```cpp
    GetMooncake().installApp(std::make_unique<AppSettings>());
```

(add after the existing `AppPendulum` install line)

- [ ] **Step 8: Reconfigure and build**

```bash
. ~/esp/esp-idf/export.sh
idf.py reconfigure
idf.py build
```

Expected: builds cleanly. If it doesn't, see the note in Step 6 about
`fmt::format`/`common::FirmwareVersion` includes first before investigating
further.

- [ ] **Step 9: Flash and visually verify**

```bash
lsof /dev/cu.usbmodem1101
idf.py -p /dev/cu.usbmodem1101 flash
```

Open Settings from the launcher. Expected: a scrollable dark menu with
"Device" (Brightness/Volume/Button buttons), "Time & Date" (Set Time/Set
Date buttons), and "Firmware" (a version-number button) sections. Tapping
any button does nothing yet (that's expected — Tasks 2-4 wire them up).
Confirm holding both buttons returns to the launcher cleanly.

- [ ] **Step 10: Commit**

```bash
git add main/assets/fonts/CommissionerMedium108.c main/assets/images/icon_settings_app.c \
        main/assets/assets.h main/apps/app_settings main/apps/apps.h main/main.cpp
git commit -m "Add Settings app skeleton with menu (ported from M5StopWatch-UserDemo)"
```

---

### Task 2: Device section (Brightness, Volume, Button)

**Files:**
- Create: `main/apps/app_settings/workers/device.cpp` (copied verbatim)
- Modify: `main/apps/app_settings/app_settings.cpp`

**Interfaces:**
- Consumes: `GetHAL().getBackLightBrightness/setBackLightBrightness`,
  `getSpeakerVolume/setSpeakerVolume`, `getButtonConfig/setButtonConfig`
  (all already in `main/hal/hal.h`); `Slider`, `Switch` widgets (already in
  `components/smooth_ui_toolkit`).
- Produces: `BrightnessWorker`, `VolumeWorker`, `ButtonWorker` become fully
  functional (declared in Task 1's `workers.h`, defined here).

- [ ] **Step 1: Copy the device worker implementations verbatim**

```bash
cp /Users/wenhuabin/Project/M5StopWatch-UserDemo/main/apps/app_setup/workers/device.cpp \
   /Users/wenhuabin/Project/m5-stopwatch/main/apps/app_settings/workers/device.cpp
```

Confirm it's self-contained (expect no output):

```bash
grep -n "app_setup\|AppSetup" /Users/wenhuabin/Project/m5-stopwatch/main/apps/app_settings/workers/device.cpp
```

- [ ] **Step 2: Wire the Device section's lambdas** — in
  `main/apps/app_settings/app_settings.cpp`'s `onOpen()`, replace the
  `"Device"` section:

```cpp
        {
            "Device",
            {
                {"Brightness",
                 [&]() {
                     _destroy_menu = true;
                     _worker       = std::make_unique<BrightnessWorker>();
                 }},
                {"Volume",
                 [&]() {
                     _destroy_menu = true;
                     _worker       = std::make_unique<VolumeWorker>();
                 }},
                {"Button",
                 [&]() {
                     _destroy_menu = true;
                     _worker       = std::make_unique<ButtonWorker>();
                 }},
            },
        },
```

- [ ] **Step 3: Reconfigure and build**

```bash
idf.py reconfigure
idf.py build
```

Expected: builds cleanly.

- [ ] **Step 4: Flash and verify on hardware**

```bash
lsof /dev/cu.usbmodem1101
idf.py -p /dev/cu.usbmodem1101 flash
```

Open Settings -> Device -> Brightness: a slider should show the current
backlight level; dragging it should visibly change screen brightness live;
tapping OK should return to the menu. Repeat for Volume (check via a sound
that uses `GetHAL()`'s speaker, e.g. a button-press SFX if enabled) and
Button (two switches for SFX/Vibration; toggling should be reflected in
subsequent button presses' feedback).

- [ ] **Step 5: Commit**

```bash
git add main/apps/app_settings
git commit -m "Wire up Brightness/Volume/Button in Settings"
```

---

### Task 3: Time & Date section

**Files:**
- Create: `main/apps/app_settings/workers/datetime.cpp` (copied verbatim)
- Modify: `main/apps/app_settings/app_settings.cpp`

**Interfaces:**
- Consumes: `GetHAL().getTimeHms/setTimeHms`, `getDateYmd/setDateYmd`
  (already in `main/hal/hal.h`); `Roller` widget (already in
  `components/smooth_ui_toolkit`).
- Produces: `SetTimeWorker`, `SetDateWorker` become fully functional.

- [ ] **Step 1: Copy the datetime worker implementations verbatim**

```bash
cp /Users/wenhuabin/Project/M5StopWatch-UserDemo/main/apps/app_setup/workers/datetime.cpp \
   /Users/wenhuabin/Project/m5-stopwatch/main/apps/app_settings/workers/datetime.cpp
```

Confirm it's self-contained (expect no output):

```bash
grep -n "app_setup\|AppSetup" /Users/wenhuabin/Project/m5-stopwatch/main/apps/app_settings/workers/datetime.cpp
```

- [ ] **Step 2: Wire the Time & Date section's lambdas** — in
  `main/apps/app_settings/app_settings.cpp`'s `onOpen()`, replace the
  `"Time & Date"` section:

```cpp
        {
            "Time & Date",
            {
                {"Set Time",
                 [&]() {
                     _destroy_menu = true;
                     _worker       = std::make_unique<SetTimeWorker>();
                 }},
                {"Set Date",
                 [&]() {
                     _destroy_menu = true;
                     _worker       = std::make_unique<SetDateWorker>();
                 }},
            },
        },
```

- [ ] **Step 3: Reconfigure and build**

```bash
idf.py reconfigure
idf.py build
```

Expected: builds cleanly.

- [ ] **Step 4: Flash and verify on hardware**

```bash
lsof /dev/cu.usbmodem1101
idf.py -p /dev/cu.usbmodem1101 flash
```

Open Settings -> Time & Date -> Set Time: three infinite rollers
(hour/minute/second) should show the current time; scroll them to a new
value, tap OK, and confirm the change stuck (reopen Set Time to check, or
open the Pendulum app and confirm its clock face now shows the new time).
Repeat for Set Date: year/month rollers first ("Next" button), then a day
roller clamped to the selected month's actual day count (e.g. Feb should
cap at 28/29), tap OK, confirm it stuck.

- [ ] **Step 5: Commit**

```bash
git add main/apps/app_settings
git commit -m "Wire up Set Time/Set Date in Settings"
```

---

### Task 4: Firmware section + About easter egg

**Files:**
- Create: `main/apps/app_settings/workers/about.cpp` (copied verbatim)
- Modify: `main/apps/app_settings/app_settings.cpp`

**Interfaces:**
- Consumes: `Random::getInstance()` (already in
  `components/smooth_ui_toolkit/src/tools/random/random.hpp`).
- Produces: `AboutWorker` becomes fully functional; tapping the firmware
  version menu item 10 times opens it.

- [ ] **Step 1: Copy the about worker implementation verbatim**

```bash
cp /Users/wenhuabin/Project/M5StopWatch-UserDemo/main/apps/app_setup/workers/about.cpp \
   /Users/wenhuabin/Project/m5-stopwatch/main/apps/app_settings/workers/about.cpp
```

Confirm it's self-contained (expect no output):

```bash
grep -n "app_setup\|AppSetup" /Users/wenhuabin/Project/m5-stopwatch/main/apps/app_settings/workers/about.cpp
```

- [ ] **Step 2: Wire the Firmware section's magic-tap lambda** — in
  `main/apps/app_settings/app_settings.cpp`'s `onOpen()`, replace the
  `"Firmware"` section:

```cpp
        {
            "Firmware",
            {
                {fmt::format("Version: {}", common::FirmwareVersion),
                 [&]() {
                     _magic_count++;
                     if (_magic_count >= 10) {
                         _magic_count  = 0;
                         _destroy_menu = true;
                         _worker       = std::make_unique<AboutWorker>();
                     }
                 }},
            },
        },
```

- [ ] **Step 3: Reconfigure and build**

```bash
idf.py reconfigure
idf.py build
```

Expected: builds cleanly.

- [ ] **Step 4: Flash and verify on hardware**

```bash
lsof /dev/cu.usbmodem1101
idf.py -p /dev/cu.usbmodem1101 flash
```

Open Settings -> Firmware -> tap the version button 10 times. Expected: a
blue "fake BSOD" screen appears with a ":(" face and a progress percentage
that climbs on its own over a few seconds. Holding both buttons should
still return to the launcher from this screen (it's just another app
state, not a real crash). Re-verify the rest of the menu (Brightness,
Volume, Button, Set Time, Set Date) still works after this — the
`_magic_count` reset logic shouldn't affect other sections.

- [ ] **Step 5: Commit**

```bash
git add main/apps/app_settings
git commit -m "Wire up the About easter egg in Settings"
```
