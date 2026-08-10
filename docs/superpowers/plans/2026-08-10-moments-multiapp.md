# Moments Multi-App Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the single-script `Moments.py` MicroPython app with a C++/ESP-IDF multi-app firmware (mooncake framework, forked from `M5StopWatch-UserDemo`) where Moments is the first installed app, and the launcher can host more apps later.

**Architecture:** Fork `M5StopWatch-UserDemo`'s ESP-IDF project as the baseline (mooncake app framework + LVGL + M5GFX HAL), prune every bundled example app down to just the launcher + `app_template` + core HAL, then add a new `app_moments` module that ports the Python Moments feature set: full-screen photo slideshow with touch-based prev/next, and a long-press-triggered "upload mode" that opens a Wi-Fi AP + HTTP server (modeled on the demo's Badge app / `config_ap` pattern) so a phone browser can push up to 5 photos.

**Tech Stack:** ESP-IDF v5.5.4 (C++17), `mooncake`/`mooncake_log` app framework, LVGL v9 (via `smooth_ui_toolkit`'s `uitk::lvgl_cpp` wrapper), M5GFX, `esp_wifi` + `esp_http_server` + `78/esp-wifi-connect`'s `dns_server` for the AP/upload server, ESP-IDF `wear_levelling` FATFS mounted at `/spiflash`.

## Global Constraints

- Target hardware: M5Stack StopWatch, ESP32-S3, 466x466 round touch display, physical BtnA/BtnB.
- ESP-IDF version: v5.5.4 exactly (per `M5StopWatch-UserDemo/README.md`).
- Only `AppLauncher` and `AppMoments` are installed in `main.cpp` — no other example apps survive the port.
- Moments upload mode is AP-only (no STA/pre-configured-Wi-Fi path), matching `config_ap`'s convention. SSID pattern: `M5StopWatch-<MAC-suffix>-Moments`.
- Photos are stored as exact 466x466 uncompressed BMP files under `/spiflash/moments`; the upload HTML page resizes/encodes client-side so the watch never decodes JPEG/PNG on-device.
- Max 5 photos per batch, 2 MiB max per uploaded file; a new upload batch fully replaces the previous one.
- Every task ends with `idf.py build` succeeding — no on-device flashing/testing is available in this environment.
- Follow existing code conventions exactly: `uitk::lvgl_cpp` wrapper classes (`Container`, `Label`, `Button`, `Image`), `LvglLockGuard`, `mclog::tagInfo/tagWarn/tagError`, `input::KeyManager`, file layout `apps/app_xxx/{app_xxx.h,app_xxx.cpp,view/view.h,view/view.cpp}`.

---

## Task 1: Bootstrap the ESP-IDF baseline

**Files:**
- Create: everything copied from `/Users/wenhuabin/Project/M5StopWatch-UserDemo` into `/Users/wenhuabin/Project/m5-stopwatch` (`CMakeLists.txt`, `partitions.csv`, `sdkconfig.defaults`, `repos.json`, `fetch_repos.py`, `README.md`, `.github/`, `LICENSE`, `dependencies.lock`, `patches/`, `main/`)
- Delete: `Moments.py`, `memory.m5f2`
- Modify: `CMakeLists.txt` (project name), `README.md` (project name/description)

**Interfaces:** N/A (infrastructure task).

- [ ] **Step 1: Copy the demo project as the baseline**

```bash
cd /Users/wenhuabin/Project
rsync -a --exclude='.git' M5StopWatch-UserDemo/ m5-stopwatch/
```

- [ ] **Step 2: Remove the old MicroPython artifacts**

`Moments.py` and `memory.m5f2` were never committed (they predate this
repo's git history), so a plain filesystem removal is enough — there's
nothing to unstage:

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
rm -f Moments.py memory.m5f2
```

- [ ] **Step 3: Rename the project**

In `CMakeLists.txt`, change:

```cmake
project(StopWatch-UserDemo)
```

to:

```cmake
project(m5-stopwatch)
```

- [ ] **Step 4: Update the README**

Replace the contents of `README.md` with:

```markdown
# M5 StopWatch Firmware

ESP-IDF multi-app firmware for the M5Stack StopWatch, built on the
`mooncake` app framework. Apps live under `main/apps/`; `Moments` (a
5-photo slideshow with phone-browser upload) is the first installed app
beyond the launcher.

## Build

### Fetch Dependencies

\`\`\`bash
python3 ./fetch_repos.py
\`\`\`

### Tool Chains

[ESP-IDF v5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/index.html)

### Build

\`\`\`bash
idf.py build
\`\`\`

### Flash

\`\`\`bash
idf.py flash
\`\`\`
```

- [ ] **Step 5: Install ESP-IDF v5.5.4 (skip if already installed)**

```bash
ls ~/esp/esp-idf/export.sh 2>/dev/null || (
  mkdir -p ~/esp && cd ~/esp && \
  git clone -b v5.5.4 --recursive https://github.com/espressif/esp-idf.git && \
  cd esp-idf && ./install.sh esp32s3
)
```

Expected: either the `ls` finds an existing install, or the clone+install
completes without error (this step can take 10-20 minutes and needs a few
GB of disk on first run).

- [ ] **Step 6: Fetch component dependencies**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
. ~/esp/esp-idf/export.sh
python3 ./fetch_repos.py
```

Expected: `components/mooncake`, `components/mooncake_log`,
`components/smooth_ui_toolkit`, `components/M5GFX`, `components/ArduinoJson`,
`components/lvgl`, `components/M5IOE1`, `components/M5PM1`,
`components/BMI270_BMM150_Sensor` all exist afterward.

- [ ] **Step 7: Baseline build verification**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
. ~/esp/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
```

Expected: `Project build complete.` This confirms the copied (still
unpruned) project builds cleanly in this environment before any of our
edits — any failure here is an environment/toolchain problem, not a
regression from later tasks.

- [ ] **Step 8: Commit**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
git add -A -- ':!components' ':!build' ':!sdkconfig' ':!sdkconfig.old'
git commit -m "$(cat <<'EOF'
Bootstrap ESP-IDF baseline from M5StopWatch-UserDemo

Copies the demo's mooncake/LVGL firmware project as the starting point
for the multi-app rewrite; drops the old MicroPython Moments.py script.
EOF
)"
```

---

## Task 2: Prune to framework-only (launcher + template, no example apps)

**Files:**
- Delete: `main/apps/app_watch_face/`, `main/apps/app_stopwatch/`, `main/apps/app_badge/`, `main/apps/app_imu/`, `main/apps/app_fft/`, `main/apps/app_lucky_wheel/`, `main/apps/app_alarm_clock/`, `main/apps/app_setup/`, `main/hal/utils/config_ap/`, `main/hal/hal_badge.cpp`, `main/hal/hal_alarm.cpp`
- Delete assets: `main/assets/images/icon_clock.c`, `icon_setup.c`, `icon_imu.c`, `icon_fft.c`, `icon_stopwatch.c`, `icon_badge.c`, `icon_lucky_wheel.c`, `icon_watch_face.c`, `icon_edit_badge.c`, `alarm_icon.c`, `lucky_wheel_pointer.c`, `main/assets/images/watch_face/` (whole dir), `main/assets/fonts/CommissionerMedium64.c`, `CommissionerMedium108.c`, `lv_font_maple_mono_medium_24.c`, `lv_font_maple_mono_medium_48.c`
- Modify: `main/apps/apps.h`, `main/main.cpp`, `main/assets/assets.h`, `main/hal/hal.h`

**Interfaces:** N/A — this task only removes code and updates the two files that reference the removed apps/assets.

- [ ] **Step 1: Delete the example app directories**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
git rm -rf \
  main/apps/app_watch_face \
  main/apps/app_stopwatch \
  main/apps/app_badge \
  main/apps/app_imu \
  main/apps/app_fft \
  main/apps/app_lucky_wheel \
  main/apps/app_alarm_clock \
  main/apps/app_setup \
  main/hal/utils/config_ap \
  main/hal/hal_badge.cpp \
  main/hal/hal_alarm.cpp
```

- [ ] **Step 2: Delete the now-unused assets**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
git rm -rf \
  main/assets/images/icon_clock.c \
  main/assets/images/icon_setup.c \
  main/assets/images/icon_imu.c \
  main/assets/images/icon_fft.c \
  main/assets/images/icon_stopwatch.c \
  main/assets/images/icon_badge.c \
  main/assets/images/icon_lucky_wheel.c \
  main/assets/images/icon_watch_face.c \
  main/assets/images/icon_edit_badge.c \
  main/assets/images/alarm_icon.c \
  main/assets/images/lucky_wheel_pointer.c \
  main/assets/images/watch_face \
  main/assets/fonts/CommissionerMedium64.c \
  main/assets/fonts/CommissionerMedium108.c \
  main/assets/fonts/lv_font_maple_mono_medium_24.c \
  main/assets/fonts/lv_font_maple_mono_medium_48.c
```

`go_home_guide.c`, `icon_indicator_left.c`, `icon_indicator_right.c`,
`icon_bat_lightning.c`, `MontserratSemiBold26.c`, and
`lv_font_maple_mono_medium_28.c` stay — they're used by
`app_launcher`/`apps/common`.

- [ ] **Step 3: Update `main/apps/apps.h`**

Replace its contents with:

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "app_template/app_template.h"
#include "app_launcher/app_launcher.h"
#include "app_moments/app_moments.h"
```

(`app_moments/app_moments.h` doesn't exist yet — it's created in Task 4.
This will not compile until then; that's expected and fixed within this
same pruning pass by temporarily commenting it out — see Step 4.)

- [ ] **Step 4: Temporarily comment out the not-yet-created app_moments include**

Since `AppMoments` doesn't exist until Task 4, comment that line out for
now so this task's build passes on its own:

```cpp
#pragma once
#include "app_template/app_template.h"
#include "app_launcher/app_launcher.h"
// #include "app_moments/app_moments.h"  // added in Task 4
```

- [ ] **Step 5: Update `main/main.cpp`**

Replace the `app_main` body's app-install section:

```cpp
    // Install apps
    GetMooncake().installApp(std::make_unique<AppLauncher>());
    GetMooncake().installApp(std::make_unique<AppAlarmClock>());
    GetMooncake().installApp(std::make_unique<AppWatchFace>());
    GetMooncake().installApp(std::make_unique<AppStopWatch>());
    GetMooncake().installApp(std::make_unique<AppBadge>());
    GetMooncake().installApp(std::make_unique<AppImu>());
    GetMooncake().installApp(std::make_unique<AppFft>());
    GetMooncake().installApp(std::make_unique<AppLuckyWheel>());
    GetMooncake().installApp(std::make_unique<AppSetup>());
    // GetMooncake().installApp(std::make_unique<AppTemplate>());
```

with:

```cpp
    // Install apps
    GetMooncake().installApp(std::make_unique<AppLauncher>());
    // GetMooncake().installApp(std::make_unique<AppMoments>());  // added in Task 4
    // GetMooncake().installApp(std::make_unique<AppTemplate>());
```

- [ ] **Step 6: Update `main/assets/assets.h`**

Replace its contents with:

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <lvgl.h>

LV_FONT_DECLARE(MontserratSemiBold26);
LV_FONT_DECLARE(lv_font_maple_mono_medium_28);

LV_IMG_DECLARE(icon_indicator_left);
LV_IMG_DECLARE(icon_indicator_right);
LV_IMG_DECLARE(go_home_guide);
LV_IMG_DECLARE(icon_bat_lightning);
```

- [ ] **Step 7: Remove the alarm and badge sections from `main/hal/hal.h`**

Delete the `AlarmStorageEntry` and `AlarmStorageSnapshot` structs near the
top of the file (the two structs immediately before `TimeHms`), and
inside `class Hal`, delete:

```cpp
    bool loadAlarmStorage(AlarmStorageSnapshot& snapshot);
    bool saveAlarmStorage(const AlarmStorageSnapshot& snapshot);
    void startAlarm();
    void stopAlarm();
```

(leave `syncRtcTimeToSystem`, `syncSystemTimeToRtc`, `getDateYmd`,
`setDateYmd`, `getTimeHms`, `setTimeHms`, `setTimezone`, `getTimezone` —
those are plain RTC access, not alarm-specific) and delete:

```cpp
    /* ---------------------------------- Badge --------------------------------- */
    bool loadBadgeImage(lv_obj_t* image);
    bool loadNextBadgeImage(lv_obj_t* image);
    bool loadPreviousBadgeImage(lv_obj_t* image);
    void startBadgeEditModeViaAp(std::function<void(std::string_view)> onLog);
```

- [ ] **Step 8: Build verification**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
. ~/esp/esp-idf/export.sh
idf.py build
```

Expected: `Project build complete.` The launcher now shows an empty grid
(no apps installed besides itself yet) — that's expected until Task 4.

- [ ] **Step 9: Commit**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
git add -A -- ':!components' ':!build' ':!sdkconfig' ':!sdkconfig.old'
git commit -m "$(cat <<'EOF'
Prune example apps down to launcher + template framework

Removes every bundled demo app (watch face, stopwatch, badge, imu, fft,
lucky wheel, alarm clock, setup) and their dedicated HAL/assets, keeping
only mooncake, the launcher, app_template, and shared apps/common/ and
core HAL utilities as the base for the Moments app.
EOF
)"
```

---

## Task 3: Moments storage layer

**Files:**
- Create: `main/apps/app_moments/storage/moments_storage.h`
- Create: `main/apps/app_moments/storage/moments_storage.cpp`

**Interfaces:**
- Produces (used by Task 4's `AppMoments` and Task 7's `upload_server`):
  - `moments::storage::kMomentsDir` (`const char*`), `kMaxPhotos` (`std::size_t` = 5), `kPhotoWidth`/`kPhotoHeight` (`int` = 466)
  - `void moments::storage::ensure_dir()`
  - `std::vector<std::string> moments::storage::load_existing_photos()`
  - `void moments::storage::delete_all_photos()`
  - `std::string moments::storage::make_photo_path(uint32_t batch_id, std::size_t index)`
  - `std::string moments::storage::temp_upload_path()`
  - `bool moments::storage::read_bmp_dimensions(const std::string& path, int& width, int& height)`
  - `long moments::storage::free_space_bytes()`

- [ ] **Step 1: Create the storage header**

`main/apps/app_moments/storage/moments_storage.h`:

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace moments::storage {

constexpr const char* kMomentsDir = "/spiflash/moments";
constexpr std::size_t kMaxPhotos  = 5;
constexpr int kPhotoWidth         = 466;
constexpr int kPhotoHeight        = 466;

// Creates the moments directory if missing and clears any leftover
// in-progress upload temp file.
void ensure_dir();

// Returns up to kMaxPhotos valid (exact-dimension BMP) photo paths,
// sorted by filename. Any other file found in the directory is treated
// as stale/corrupt and deleted.
std::vector<std::string> load_existing_photos();

// Deletes every file in the moments directory.
void delete_all_photos();

// Absolute path a photo at `index` within batch `batch_id` should be
// stored at.
std::string make_photo_path(uint32_t batch_id, std::size_t index);

// Path used to stream an in-progress upload before it's renamed into
// place.
std::string temp_upload_path();

// Reads just the BMP header (26 bytes) to get width/height without
// loading the whole file. Returns false if `path` isn't a valid BMP.
bool read_bmp_dimensions(const std::string& path, int& width, int& height);

// Free bytes on the partition backing kMomentsDir, or -1 if it can't be
// determined.
long free_space_bytes();

}  // namespace moments::storage
```

- [ ] **Step 2: Create the storage implementation**

`main/apps/app_moments/storage/moments_storage.cpp`:

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "moments_storage.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <esp_vfs_fat.h>
#include <mooncake_log.h>

namespace moments::storage {
namespace {

constexpr const char* _tag       = "MomentsStorage";
constexpr const char* _temp_name = "upload.tmp";

bool has_bmp_extension(const std::string& name)
{
    if (name.size() < 4) {
        return false;
    }
    std::string ext = name.substr(name.size() - 4);
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext == ".bmp";
}

}  // namespace

std::string temp_upload_path()
{
    return std::string(kMomentsDir) + "/" + _temp_name;
}

void ensure_dir()
{
    if (mkdir(kMomentsDir, 0775) != 0 && errno != EEXIST) {
        mclog::tagError(_tag, "failed to create {}: errno={}", kMomentsDir, errno);
    }
    unlink(temp_upload_path().c_str());
}

std::string make_photo_path(uint32_t batch_id, std::size_t index)
{
    char path[96] = {};
    snprintf(path, sizeof(path), "%s/moment_%010u_%u.bmp", kMomentsDir, static_cast<unsigned>(batch_id),
              static_cast<unsigned>(index));
    return std::string(path);
}

bool read_bmp_dimensions(const std::string& path, int& width, int& height)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }

    unsigned char header[26] = {};
    const std::size_t read_size = fread(header, 1, sizeof(header), file);
    fclose(file);

    if (read_size != sizeof(header) || header[0] != 'B' || header[1] != 'M') {
        return false;
    }

    width  = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
    height = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
    return true;
}

std::vector<std::string> load_existing_photos()
{
    std::vector<std::string> photos;

    DIR* dir = opendir(kMomentsDir);
    if (dir == nullptr) {
        return photos;
    }

    std::vector<std::string> candidates;
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        const std::string name = entry->d_name;
        if (name == "." || name == ".." || name == _temp_name) {
            continue;
        }
        candidates.push_back(name);
    }
    closedir(dir);

    std::sort(candidates.begin(), candidates.end());

    for (const auto& name : candidates) {
        const std::string path = std::string(kMomentsDir) + "/" + name;

        int width  = 0;
        int height = 0;
        const bool valid = photos.size() < kMaxPhotos && has_bmp_extension(name) &&
                            read_bmp_dimensions(path, width, height) && width == kPhotoWidth &&
                            height == kPhotoHeight;

        if (valid) {
            photos.push_back(path);
        } else {
            mclog::tagWarn(_tag, "dropping invalid moments file: {}", path);
            unlink(path.c_str());
        }
    }

    return photos;
}

void delete_all_photos()
{
    DIR* dir = opendir(kMomentsDir);
    if (dir == nullptr) {
        return;
    }

    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        unlink((std::string(kMomentsDir) + "/" + name).c_str());
    }
    closedir(dir);
}

long free_space_bytes()
{
    uint64_t total_bytes = 0;
    uint64_t free_bytes  = 0;
    if (esp_vfs_fat_info("/spiflash", &total_bytes, &free_bytes) != ESP_OK) {
        return -1;
    }
    return static_cast<long>(free_bytes);
}

}  // namespace moments::storage
```

- [ ] **Step 3: Build verification**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
. ~/esp/esp-idf/export.sh
idf.py build
```

Expected: `Project build complete.` (`main/CMakeLists.txt` already globs
`apps/*.cpp` recursively, so this new file is picked up automatically;
nothing calls it yet, but it must still compile standalone.)

- [ ] **Step 4: Commit**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
git add main/apps/app_moments/storage
git commit -m "$(cat <<'EOF'
Add Moments photo storage layer

Manages /spiflash/moments: loading and validating existing exact-466x466
BMP photos, deleting a batch, and building paths for new uploads. Ported
from the equivalent logic in the old Moments.py.
EOF
)"
```

---

## Task 4: Icon asset + AppMoments/MomentsView skeleton wired into the launcher

**Files:**
- Create: `main/assets/images/icon_moments.c` (generated)
- Modify: `main/assets/assets.h`
- Create: `main/apps/app_moments/view/view.h`
- Create: `main/apps/app_moments/view/view.cpp`
- Create: `main/apps/app_moments/app_moments.h`
- Create: `main/apps/app_moments/app_moments.cpp`
- Modify: `main/apps/apps.h`, `main/main.cpp` (uncomment the lines stubbed out in Task 2)

**Interfaces:**
- Consumes: `moments::storage::load_existing_photos()`, `kMomentsDir`, `kPhotoWidth`/`kPhotoHeight` (Task 3); `input::KeyManager`, `input::KeyEvent::GoHome` (existing `apps/common/key_manager`); `LvglLockGuard`, `GetHAL()` (existing `hal/hal.h`)
- Produces (used by Task 5):
  - `class view::MomentsView` with `void init(lv_obj_t* parent = lv_screen_active())`, `void setPhotos(const std::vector<std::string>& photoPaths)`, `void showNext()`, `void showPrevious()`
  - `class AppMoments : public mooncake::AppAbility`

- [ ] **Step 1: Write the icon generator script and run it**

`/private/tmp/claude-501/-Users-wenhuabin-Project-m5-stopwatch/f8301831-70ba-48b3-b8a0-80bee4378fe1/scratchpad/gen_icon_moments.py`:

```python
#!/usr/bin/env python3
"""Generates main/assets/images/icon_moments.c: a 200x200 RGB565 LVGL
image descriptor, matching the format LVGL's image converter produces
(see icon_stopwatch.c for reference). Draws a simple stacked-photos
glyph: two overlapping rounded squares on a transparent-looking dark
background, in the app's accent color.
"""
import struct

WIDTH = 200
HEIGHT = 200
BG = (0x11, 0x14, 0x1c)
ACCENT = (0x58, 0x65, 0xf2)
ACCENT_DIM = (0x35, 0x3c, 0x8f)


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def in_rounded_rect(x, y, x0, y0, x1, y1, radius):
    if x0 <= x < x1 and y0 <= y < y1:
        cx = min(max(x, x0 + radius), x1 - radius - 1)
        cy = min(max(y, y0 + radius), y1 - radius - 1)
        return (x - cx) ** 2 + (y - cy) ** 2 <= radius * radius
    return False


def pixel(x, y):
    back_rect = (46, 56, 154, 144)
    front_rect = (60, 70, 168, 158)

    if in_rounded_rect(x, y, *front_rect, 18):
        return ACCENT
    if in_rounded_rect(x, y, *back_rect, 18):
        return ACCENT_DIM
    return BG


def main():
    pixels = bytearray()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            r, g, b = pixel(x, y)
            pixels += struct.pack("<H", rgb565(r, g, b))

    with open("main/assets/images/icon_moments.c", "w") as out:
        out.write(
            '#ifdef __has_include\n'
            '#if __has_include("lvgl.h")\n'
            '#ifndef LV_LVGL_H_INCLUDE_SIMPLE\n'
            '#define LV_LVGL_H_INCLUDE_SIMPLE\n'
            '#endif\n'
            '#endif\n'
            '#endif\n\n'
            '#if defined(LV_LVGL_H_INCLUDE_SIMPLE)\n'
            '#include "lvgl.h"\n'
            '#else\n'
            '#include "lvgl/lvgl.h"\n'
            '#endif\n\n'
            '#ifndef LV_ATTRIBUTE_MEM_ALIGN\n'
            '#define LV_ATTRIBUTE_MEM_ALIGN\n'
            '#endif\n\n'
            '#ifndef LV_ATTRIBUTE_IMAGE_ICON_MOMENTS\n'
            '#define LV_ATTRIBUTE_IMAGE_ICON_MOMENTS\n'
            '#endif\n\n'
            'const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_ICON_MOMENTS '
            'uint8_t icon_moments_map[] = {\n'
        )

        for i in range(0, len(pixels), 19):
            chunk = pixels[i:i + 19]
            out.write("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")

        out.write(
            "};\n\n"
            "const lv_image_dsc_t icon_moments = {\n"
            "    .header.cf    = LV_COLOR_FORMAT_RGB565,\n"
            "    .header.magic = LV_IMAGE_HEADER_MAGIC,\n"
            f"    .header.w     = {WIDTH},\n"
            f"    .header.h     = {HEIGHT},\n"
            f"    .data_size    = {WIDTH * HEIGHT} * 2,\n"
            "    .data         = icon_moments_map,\n"
            "};\n"
        )


if __name__ == "__main__":
    main()
```

Run it:

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
python3 /private/tmp/claude-501/-Users-wenhuabin-Project-m5-stopwatch/f8301831-70ba-48b3-b8a0-80bee4378fe1/scratchpad/gen_icon_moments.py
```

Expected: `main/assets/images/icon_moments.c` is created (roughly 4200
lines, matching `icon_stopwatch.c`'s shape).

- [ ] **Step 2: Declare the icon in `main/assets/assets.h`**

Add, next to the other `LV_IMG_DECLARE` lines:

```cpp
LV_IMG_DECLARE(icon_moments);
```

- [ ] **Step 3: Write the Moments view header**

`main/apps/app_moments/view/view.h`:

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <memory>
#include <smooth_lvgl.hpp>
#include <string>
#include <uitk/short_namespace.hpp>
#include <vector>

namespace view {

class MomentsUploadDialog {
public:
    void init(lv_obj_t* parent);
    bool isConfirmed() const
    {
        return _is_confirmed;
    }
    bool isCancelled() const
    {
        return _is_cancelled;
    }

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _label;
    std::unique_ptr<uitk::lvgl_cpp::Button> _confirm_button;
    std::unique_ptr<uitk::lvgl_cpp::Button> _cancel_button;
    bool _is_confirmed = false;
    bool _is_cancelled = false;
};

enum class TapSide {
    None,
    Left,
    Right,
};

class MomentsView {
public:
    void init(lv_obj_t* parent = lv_screen_active());

    // Replaces the displayed photo list and shows the first photo (or
    // the empty-state hint if `photoPaths` is empty).
    void setPhotos(const std::vector<std::string>& photoPaths);

    void showNext();
    void showPrevious();

    // Runs pending dialog state transitions. Call once per app loop
    // iteration.
    void update();

    // Consumes (and clears) a pending tap-navigation request.
    TapSide consumeTap();

    // Consumes (and clears) a pending "user confirmed upload" request.
    bool consumeUploadRequested();

private:
    void showPhoto(std::size_t index);
    void showUploadDialog();
    static void handleClicked(lv_event_t* e);
    static void handleLongPressed(lv_event_t* e);

    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Image> _image;
    std::unique_ptr<uitk::lvgl_cpp::Label> _empty_hint_label;
    std::unique_ptr<MomentsUploadDialog> _upload_dialog;

    std::vector<std::string> _photo_paths;
    std::size_t _photo_index = 0;
    TapSide _pending_tap     = TapSide::None;
    bool _upload_requested   = false;
};

}  // namespace view
```

- [ ] **Step 4: Write the Moments view implementation**

`main/apps/app_moments/view/view.cpp`:

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"

using namespace view;
using namespace uitk::lvgl_cpp;

namespace {

constexpr int _panel_size           = 466;
constexpr int _hint_width           = 320;
constexpr int _dialog_width         = 404;
constexpr int _dialog_height        = 202;
constexpr int _dialog_button_width  = 148;
constexpr int _dialog_button_height = 63;

constexpr uint32_t _dialog_bg_color      = 0x2B2B2B;
constexpr uint32_t _dialog_border_color  = 0x6A6A6A;
constexpr uint32_t _dialog_confirm_color = 0x5865F2;
constexpr uint32_t _dialog_cancel_color  = 0x515151;
constexpr uint32_t _label_color          = 0xFFFFFF;
constexpr uint32_t _hint_text_color      = 0xD9D9D9;

}  // namespace

void MomentsUploadDialog::init(lv_obj_t* parent)
{
    _is_confirmed = false;
    _is_cancelled = false;

    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_dialog_width, _dialog_height);
    _panel->setBgColor(lv_color_hex(_dialog_bg_color));
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->setBorderColor(lv_color_hex(_dialog_border_color));
    _panel->setBorderWidth(3);
    _panel->setRadius(58);
    _panel->setPaddingAll(0);
    _panel->moveForeground();

    _label = std::make_unique<Label>(_panel->get());
    _label->setText("Upload new photos?");
    _label->setTextFont(&MontserratSemiBold26);
    _label->setTextColor(lv_color_hex(_label_color));
    _label->align(LV_ALIGN_TOP_LEFT, 49, 40);

    _confirm_button = std::make_unique<Button>(_panel->get());
    _confirm_button->setSize(_dialog_button_width, _dialog_button_height);
    _confirm_button->align(LV_ALIGN_CENTER, -92, 36);
    _confirm_button->setRadius(LV_RADIUS_CIRCLE);
    _confirm_button->setBorderWidth(0);
    _confirm_button->setShadowWidth(0);
    _confirm_button->setBgColor(lv_color_hex(_dialog_confirm_color));
    _confirm_button->label().setText("Upload");
    _confirm_button->label().setTextFont(&lv_font_montserrat_24);
    _confirm_button->label().setTextColor(lv_color_hex(_label_color));
    _confirm_button->label().align(LV_ALIGN_CENTER, 0, 0);
    _confirm_button->onClick().connect([this]() { _is_confirmed = true; });

    _cancel_button = std::make_unique<Button>(_panel->get());
    _cancel_button->setSize(_dialog_button_width, _dialog_button_height);
    _cancel_button->align(LV_ALIGN_CENTER, 92, 36);
    _cancel_button->setRadius(LV_RADIUS_CIRCLE);
    _cancel_button->setBorderWidth(0);
    _cancel_button->setShadowWidth(0);
    _cancel_button->setBgColor(lv_color_hex(_dialog_cancel_color));
    _cancel_button->label().setText("Cancel");
    _cancel_button->label().setTextFont(&lv_font_montserrat_24);
    _cancel_button->label().setTextColor(lv_color_hex(_label_color));
    _cancel_button->label().align(LV_ALIGN_CENTER, 0, 0);
    _cancel_button->onClick().connect([this]() { _is_cancelled = true; });
}

void MomentsView::init(lv_obj_t* parent)
{
    _photo_paths.clear();
    _photo_index       = 0;
    _pending_tap       = TapSide::None;
    _upload_requested  = false;

    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_panel_size, _panel_size);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->setBgColor(lv_color_black());
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _panel->addEventCb(handleClicked, LV_EVENT_CLICKED, this);
    _panel->addEventCb(handleLongPressed, LV_EVENT_LONG_PRESSED, this);

    _image = std::make_unique<Image>(_panel->get());
    _image->align(LV_ALIGN_CENTER, 0, 0);
    _image->setHidden(true);

    _empty_hint_label = std::make_unique<Label>(_panel->get());
    _empty_hint_label->setText("No photos yet\nTap and hold to upload");
    _empty_hint_label->setTextFont(&lv_font_montserrat_24);
    _empty_hint_label->setTextColor(lv_color_hex(_hint_text_color));
    _empty_hint_label->setWidth(_hint_width);
    _empty_hint_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _empty_hint_label->align(LV_ALIGN_CENTER, 0, 0);
    _empty_hint_label->setHidden(true);
}

void MomentsView::setPhotos(const std::vector<std::string>& photoPaths)
{
    _photo_paths = photoPaths;
    showPhoto(0);
}

void MomentsView::showPhoto(std::size_t index)
{
    if (_photo_paths.empty()) {
        if (_image) {
            _image->setHidden(true);
        }
        if (_empty_hint_label) {
            _empty_hint_label->setHidden(false);
        }
        return;
    }

    _photo_index = index % _photo_paths.size();
    const std::string lvgl_path = "A:" + _photo_paths[_photo_index];
    lv_image_set_src(_image->get(), lvgl_path.c_str());
    _image->setHidden(false);
    if (_empty_hint_label) {
        _empty_hint_label->setHidden(true);
    }
}

void MomentsView::showNext()
{
    if (_photo_paths.size() < 2) {
        return;
    }
    showPhoto(_photo_index + 1);
}

void MomentsView::showPrevious()
{
    if (_photo_paths.size() < 2) {
        return;
    }
    showPhoto((_photo_index + _photo_paths.size() - 1) % _photo_paths.size());
}

void MomentsView::update()
{
    if (!_upload_dialog) {
        return;
    }

    if (_upload_dialog->isConfirmed()) {
        _upload_requested = true;
        _upload_dialog.reset();
        return;
    }

    if (_upload_dialog->isCancelled()) {
        _upload_dialog.reset();
    }
}

TapSide MomentsView::consumeTap()
{
    const TapSide tap = _pending_tap;
    _pending_tap       = TapSide::None;
    return tap;
}

bool MomentsView::consumeUploadRequested()
{
    const bool requested = _upload_requested;
    _upload_requested     = false;
    return requested;
}

void MomentsView::showUploadDialog()
{
    _upload_dialog.reset();
    _upload_dialog = std::make_unique<MomentsUploadDialog>();
    _upload_dialog->init(lv_screen_active());
}

void MomentsView::handleClicked(lv_event_t* e)
{
    auto* self = static_cast<MomentsView*>(lv_event_get_user_data(e));
    if (self == nullptr || self->_photo_paths.size() < 2) {
        return;
    }

    lv_indev_t* indev = lv_event_get_indev(e);
    if (indev == nullptr) {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    self->_pending_tap = (point.x < _panel_size / 2) ? TapSide::Left : TapSide::Right;
}

void MomentsView::handleLongPressed(lv_event_t* e)
{
    auto* self = static_cast<MomentsView*>(lv_event_get_user_data(e));
    if (self == nullptr) {
        return;
    }
    self->showUploadDialog();
}
```

Note: `view.cpp` uses `MontserratSemiBold26` (declared in `assets.h`,
kept in Task 2) and the LVGL built-in `lv_font_montserrat_24` — the same
built-in font `app_badge/view/badge.cpp`'s dialog already used
successfully in the original demo, so it's available without any
`sdkconfig.defaults` change. Add `#include <assets/assets.h>` at the top
of `view.cpp` for `MontserratSemiBold26`.

- [ ] **Step 5: Write the AppMoments header**

`main/apps/app_moments/app_moments.h`:

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "view/view.h"
#include <apps/common/key_manager/key_manager.h>
#include <memory>
#include <mooncake.h>

class AppMoments : public mooncake::AppAbility {
public:
    AppMoments();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    void reloadPhotos();

    std::unique_ptr<input::KeyManager> _key_manager;
    std::unique_ptr<view::MomentsView> _view;
};
```

- [ ] **Step 6: Write the AppMoments implementation (skeleton — no upload mode yet)**

`main/apps/app_moments/app_moments.cpp`:

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_moments.h"
#include "storage/moments_storage.h"

#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>

using namespace mooncake;

AppMoments::AppMoments()
{
    setAppInfo().name = "Moments";
    setAppInfo().icon = (void*)&icon_moments;
}

void AppMoments::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppMoments::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();

    moments::storage::ensure_dir();

    LvglLockGuard lock;
    _view = std::make_unique<view::MomentsView>();
    _view->init(lv_screen_active());
    reloadPhotos();
}

void AppMoments::onRunning()
{
    input::KeyEvent event = input::KeyEvent::None;
    if (_key_manager) {
        event = _key_manager->update();
    }

    if (event == input::KeyEvent::GoHome) {
        close();
        return;
    }

    if (!_view) {
        return;
    }

    LvglLockGuard lock;
    _view->update();
}

void AppMoments::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();

    LvglLockGuard lock;
    _view.reset();
}

void AppMoments::reloadPhotos()
{
    const auto photos = moments::storage::load_existing_photos();
    if (_view) {
        _view->setPhotos(photos);
    }
}
```

- [ ] **Step 7: Re-enable AppMoments in `main/apps/apps.h`**

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "app_template/app_template.h"
#include "app_launcher/app_launcher.h"
#include "app_moments/app_moments.h"
```

- [ ] **Step 8: Re-enable AppMoments in `main/main.cpp`**

```cpp
    // Install apps
    GetMooncake().installApp(std::make_unique<AppLauncher>());
    GetMooncake().installApp(std::make_unique<AppMoments>());
    // GetMooncake().installApp(std::make_unique<AppTemplate>());
```

- [ ] **Step 9: Build verification**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
. ~/esp/esp-idf/export.sh
idf.py build
```

Expected: `Project build complete.` The launcher now shows one tile,
"Moments," with the generated icon; opening it shows the empty-state hint
(no touch/upload behavior wired up yet — that's Tasks 5-8).

- [ ] **Step 10: Commit**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
git add main/apps/app_moments main/assets/assets.h main/assets/images/icon_moments.c main/apps/apps.h main/main.cpp
git commit -m "$(cat <<'EOF'
Add AppMoments skeleton and register it in the launcher

Moments now appears as an installed app: opens to a full-screen empty
state or the first stored photo, with a generated placeholder icon.
Touch navigation and upload mode are added in later commits.
EOF
)"
```

---

## Task 5: Touch navigation and GoHome

**Files:**
- Modify: `main/apps/app_moments/app_moments.cpp` (`onRunning`)

**Interfaces:**
- Consumes: `view::MomentsView::consumeTap()`, `showNext()`, `showPrevious()` (Task 4)

**Note:** `MomentsView` already implements tap detection (`handleClicked`)
and `showNext`/`showPrevious` from Task 4 — this task only wires
`AppMoments::onRunning` to consume the tap and act on it, matching the
original Python behavior (tap left half = previous, right half = next).

- [ ] **Step 1: Update `AppMoments::onRunning`**

In `main/apps/app_moments/app_moments.cpp`, replace:

```cpp
    LvglLockGuard lock;
    _view->update();
}
```

with:

```cpp
    LvglLockGuard lock;

    const view::TapSide tap = _view->consumeTap();
    if (tap == view::TapSide::Left) {
        _view->showPrevious();
    } else if (tap == view::TapSide::Right) {
        _view->showNext();
    }

    _view->update();
}
```

- [ ] **Step 2: Build verification**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
. ~/esp/esp-idf/export.sh
idf.py build
```

Expected: `Project build complete.`

- [ ] **Step 3: Commit**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
git add main/apps/app_moments/app_moments.cpp
git commit -m "$(cat <<'EOF'
Wire touch-based photo navigation into AppMoments

Tapping the left/right half of the screen moves to the previous/next
photo, matching the original Moments.py touch UX. GoHome (holding
BtnA+BtnB) already closes back to the launcher via KeyManager.
EOF
)"
```

---

## Task 6: Upload confirm dialog wiring (no server yet)

**Files:**
- Modify: `main/apps/app_moments/app_moments.h`, `main/apps/app_moments/app_moments.cpp`

**Interfaces:**
- Consumes: `view::MomentsView::consumeUploadRequested()` (Task 4, already implemented via `handleLongPressed`/`MomentsUploadDialog`)
- Produces (used by Task 8): a clear insertion point in `AppMoments::onRunning` where upload mode starts (currently just logs)

This task validates the long-press → dialog → confirm flow end-to-end
before adding the network code, so any LVGL event-wiring problems show up
independently of the Wi-Fi/HTTP work.

- [ ] **Step 1: Log the upload request in `AppMoments::onRunning`**

In `main/apps/app_moments/app_moments.cpp`, extend `onRunning`:

```cpp
    LvglLockGuard lock;

    const view::TapSide tap = _view->consumeTap();
    if (tap == view::TapSide::Left) {
        _view->showPrevious();
    } else if (tap == view::TapSide::Right) {
        _view->showNext();
    }

    _view->update();

    if (_view->consumeUploadRequested()) {
        mclog::tagInfo(getAppInfo().name, "upload requested (upload mode not wired up yet)");
    }
}
```

- [ ] **Step 2: Build verification**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
. ~/esp/esp-idf/export.sh
idf.py build
```

Expected: `Project build complete.`

- [ ] **Step 3: Commit**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
git add main/apps/app_moments/app_moments.cpp
git commit -m "$(cat <<'EOF'
Log upload-mode requests from the long-press confirm dialog

Confirms the long-press -> dialog -> confirm chain reaches AppMoments
before wiring in the actual AP/HTTP server in the next commit.
EOF
)"
```

---

## Task 7: Upload server (Wi-Fi AP + HTTP server + embedded HTML page)

**Files:**
- Create: `main/apps/app_moments/assets/moments_upload.html`
- Create: `main/apps/app_moments/net/upload_server.h`
- Create: `main/apps/app_moments/net/upload_server.cpp`
- Modify: `main/CMakeLists.txt` (`EMBED_TXTFILES`)

**Interfaces:**
- Consumes: `moments::storage::{kMomentsDir, kMaxPhotos, kPhotoWidth, kPhotoHeight, delete_all_photos, make_photo_path, temp_upload_path, read_bmp_dimensions, free_space_bytes}` (Task 3); `GetHAL().millis()` (existing HAL)
- Produces (used by Task 8): `void moments::net::run_upload_mode(const std::function<void(std::string_view)>& onLog)` — blocks until the phone-side "Done" button (or the AP itself) stops the session, then returns.

- [ ] **Step 1: Write the upload page**

`main/apps/app_moments/assets/moments_upload.html`:

```html
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>M5 Moments</title>
  <style>
    :root { color-scheme: dark; font-family: system-ui, sans-serif; }
    body { margin: 0; min-height: 100vh; display: grid; place-items: center;
           background: #090b10; color: #f7f7f8; }
    main { width: min(88vw, 420px); padding: 28px; border: 1px solid #30333b;
           border-radius: 22px; background: #151821; box-shadow: 0 18px 60px #0008; }
    h1 { margin: 0 0 8px; font-size: 28px; }
    p { color: #afb3bd; line-height: 1.55; }
    input { display: block; width: 100%; margin: 22px 0; }
    button { width: 100%; border: 0; border-radius: 14px; padding: 14px;
             font-size: 16px; font-weight: 700; color: white; background: #5865f2;
             margin-top: 12px; }
    button:disabled { opacity: .55; }
    button.secondary { background: #2c2f3a; }
    progress { width: 100%; margin-top: 18px; accent-color: #7c86ff; }
    #status { min-height: 24px; margin-bottom: 0; }
  </style>
</head>
<body><main>
  <h1>M5 Moments</h1>
  <p>一次选择 1～5 张图片，新图片将替换原有图片。上传后点击手表左半屏查看上一张，点击右半屏查看下一张。</p>
  <input id="files" type="file" multiple accept="image/jpeg,image/png,image/bmp,.jpg,.jpeg,.png,.bmp">
  <button id="upload">上传并播放</button>
  <button id="done" class="secondary" hidden>完成，退出上传模式</button>
  <progress id="progress" max="100" value="0" hidden></progress>
  <p id="status"></p>
</main>
<script>
const picker = document.querySelector('#files');
const button = document.querySelector('#upload');
const doneButton = document.querySelector('#done');
const status = document.querySelector('#status');
const progress = document.querySelector('#progress');

function canvasToBmp(canvas) {
  const width = canvas.width;
  const height = canvas.height;
  const pixels = canvas.getContext('2d').getImageData(0, 0, width, height).data;
  const rowSize = (width * 3 + 3) & ~3;
  const pixelBytes = rowSize * height;
  const buffer = new ArrayBuffer(54 + pixelBytes);
  const view = new DataView(buffer);
  const bytes = new Uint8Array(buffer);

  bytes[0] = 0x42; bytes[1] = 0x4d;
  view.setUint32(2, 54 + pixelBytes, true);
  view.setUint32(10, 54, true);
  view.setUint32(14, 40, true);
  view.setInt32(18, width, true);
  view.setInt32(22, height, true);
  view.setUint16(26, 1, true);
  view.setUint16(28, 24, true);
  view.setUint32(34, pixelBytes, true);
  view.setInt32(38, 2835, true);
  view.setInt32(42, 2835, true);

  for (let y = 0; y < height; y++) {
    const sourceRow = y * width * 4;
    const targetRow = 54 + (height - 1 - y) * rowSize;
    for (let x = 0; x < width; x++) {
      const source = sourceRow + x * 4;
      const target = targetRow + x * 3;
      bytes[target] = pixels[source + 2];
      bytes[target + 1] = pixels[source + 1];
      bytes[target + 2] = pixels[source];
    }
  }
  return new Blob([buffer], { type: 'image/bmp' });
}

function createWatchPreview(file) {
  return new Promise((resolve, reject) => {
    const url = URL.createObjectURL(file);
    const image = new Image();
    image.onload = () => {
      try {
        const canvas = document.createElement('canvas');
        canvas.width = 466;
        canvas.height = 466;
        const context = canvas.getContext('2d', { alpha: false });
        context.fillStyle = '#000';
        context.fillRect(0, 0, 466, 466);
        context.drawImage(image, 0, 0, 466, 466);
        const blob = canvasToBmp(canvas);
        URL.revokeObjectURL(url);
        resolve(blob);
      } catch (_) {
        URL.revokeObjectURL(url);
        reject(new Error('图片处理失败'));
      }
    };
    image.onerror = () => {
      URL.revokeObjectURL(url);
      reject(new Error(`${file.name} 无法读取`));
    };
    image.src = url;
  });
}

function sendFile(file, index, total, batch) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    let url = `/upload?batch=${encodeURIComponent(batch)}`;
    if (index === 0) url += '&reset=1';
    if (index === total - 1) url += '&last=1';
    xhr.open('POST', url);
    xhr.setRequestHeader('Content-Type', 'image/bmp');
    xhr.upload.onprogress = event => {
      if (event.lengthComputable) progress.value = (index + event.loaded / event.total) / total * 100;
    };
    xhr.onload = () => {
      let result = {}; try { result = JSON.parse(xhr.responseText); } catch (_) {}
      if (xhr.status === 200) resolve();
      else reject(new Error(result.error || '上传失败'));
    };
    xhr.onerror = () => reject(new Error('连接中断，请重试'));
    xhr.send(file);
  });
}

button.onclick = async () => {
  const files = Array.from(picker.files);
  if (!files.length) { status.textContent = '请先选择图片'; return; }
  if (files.length > 5) { status.textContent = '最多只能选择 5 张图片'; return; }

  button.disabled = true;
  doneButton.hidden = true;
  progress.hidden = false;
  progress.value = 0;
  const batch = `${Date.now()}-${Math.random().toString(36).slice(2)}`;
  try {
    for (let index = 0; index < files.length; index++) {
      status.textContent = `正在生成预览图 ${index + 1}/${files.length}…`;
      const preview = await createWatchPreview(files[index]);
      status.textContent = `正在上传 ${index + 1}/${files.length}…`;
      await sendFile(preview, index, files.length, batch);
    }
    progress.value = 100;
    status.textContent = `上传成功，共 ${files.length} 张图片`;
    doneButton.hidden = false;
  } catch (error) {
    status.textContent = error.message;
  } finally {
    button.disabled = false;
  }
};

doneButton.onclick = async () => {
  doneButton.disabled = true;
  status.textContent = '正在退出上传模式…';
  try {
    await fetch('/close', { method: 'POST' });
  } catch (_) {
    // The watch may tear down the AP before the response arrives.
  }
  status.textContent = '已完成，可以断开手机 WiFi 了';
};
</script></body></html>
```

- [ ] **Step 2: Write the upload server header**

`main/apps/app_moments/net/upload_server.h`:

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <functional>
#include <string_view>

namespace moments::net {

// Starts a Wi-Fi AP hotspot and an HTTP server that serves the upload
// page and accepts photo batches. Blocks until the phone-side "Done"
// button (POST /close) is used, then tears the AP/server down and
// returns. `onLog` is invoked with human-readable status text suitable
// for display on the watch (SSID, upload progress, errors).
void run_upload_mode(const std::function<void(std::string_view)>& onLog);

}  // namespace moments::net
```

- [ ] **Step 3: Write the upload server implementation**

`main/apps/app_moments/net/upload_server.cpp`:

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "upload_server.h"

#include "../storage/moments_storage.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unistd.h>

#include <dns_server.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <lwip/ip_addr.h>
#include <nvs_flash.h>

#include <hal/hal.h>

namespace moments::net {
namespace {

constexpr const char* _tag                = "MomentsUpload";
constexpr const char* _ap_ssid_prefix     = "M5StopWatch";
constexpr const char* _ap_ssid_suffix     = "-Moments";
constexpr const char* _ap_url             = "http://192.168.4.1";
constexpr EventBits_t _exit_requested_bit = BIT0;
constexpr std::size_t _max_upload_bytes   = 2 * 1024 * 1024;

extern const char moments_upload_html_start[] asm("_binary_moments_upload_html_start");
extern const char moments_upload_html_end[] asm("_binary_moments_upload_html_end");

constexpr const char* _captive_portal_urls[] = {
    "/hotspot-detect.html",      "/generate_204*", "/mobile/status.php",
    "/check_network_status.txt", "/ncsi.txt",      "/fwlink/",
    "/connectivity-check.html",  "/success.txt",   "/portal.html",
    "/library/test/success.html",
};

bool ensure_wifi_stack_ready()
{
    static std::mutex mutex;
    static bool initialized = false;

    std::lock_guard<std::mutex> lock(mutex);
    if (initialized) {
        return true;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(_tag, "nvs init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(_tag, "netif init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(_tag, "event loop init failed: %s", esp_err_to_name(ret));
        return false;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable         = false;
    ret                    = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(_tag, "wifi init failed: %s", esp_err_to_name(ret));
        return false;
    }

    initialized = true;
    return true;
}

std::string make_ap_ssid()
{
    uint8_t mac[6] = {};
    esp_err_t ret  = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    if (ret != ESP_OK) {
        ESP_LOGW(_tag, "read mac failed: %s", esp_err_to_name(ret));
        return std::string(_ap_ssid_prefix) + _ap_ssid_suffix;
    }

    char ssid[32] = {};
    snprintf(ssid, sizeof(ssid), "%s-%02X%02X%s", _ap_ssid_prefix, mac[4], mac[5], _ap_ssid_suffix);
    return std::string(ssid);
}

class Session {
public:
    explicit Session(const std::function<void(std::string_view)>& onLog) : _on_log(onLog) {}

    void run()
    {
        if (!ensure_wifi_stack_ready()) {
            log("Wi-Fi initialization failed");
            return;
        }

        _event_group = xEventGroupCreate();
        if (_event_group == nullptr) {
            log("Failed to create sync event group");
            return;
        }

        if (!start_access_point() || !start_web_server()) {
            stop();
            return;
        }

        log("Connect to Wi-Fi: " + _ssid + "\nThen open:\n" + std::string(_ap_url));

        xEventGroupWaitBits(_event_group, _exit_requested_bit, pdTRUE, pdFALSE, portMAX_DELAY);

        stop();

        log("Upload finished");
    }

private:
    void log(const std::string& message) const
    {
        ESP_LOGI(_tag, "%s", message.c_str());
        if (_on_log) {
            _on_log(message);
        }
    }

    bool start_access_point()
    {
        static esp_netif_t* ap_netif = nullptr;
        if (ap_netif == nullptr) {
            ap_netif = esp_netif_create_default_wifi_ap();
        }
        if (ap_netif == nullptr) {
            log("Failed to create AP network interface");
            return false;
        }

        esp_netif_ip_info_t ip_info;
        IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
        IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        esp_netif_dhcps_stop(ap_netif);
        esp_netif_set_ip_info(ap_netif, &ip_info);
        esp_netif_dhcps_start(ap_netif);

        _dns_server = std::make_unique<DnsServer>();
        _dns_server->Start(ip_info.gw);

        _ssid = make_ap_ssid();

        wifi_config_t wifi_config = {};
        strncpy(reinterpret_cast<char*>(wifi_config.ap.ssid), _ssid.c_str(), sizeof(wifi_config.ap.ssid) - 1);
        wifi_config.ap.ssid_len       = _ssid.size();
        wifi_config.ap.max_connection = 4;
        wifi_config.ap.authmode       = WIFI_AUTH_OPEN;

        esp_err_t ret = esp_wifi_stop();
        if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STARTED && ret != ESP_ERR_WIFI_MODE) {
            ESP_LOGW(_tag, "wifi stop before ap failed: %s", esp_err_to_name(ret));
        }

        ret = esp_wifi_set_mode(WIFI_MODE_AP);
        if (ret != ESP_OK) {
            log("Failed to set AP mode");
            return false;
        }

        ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
        if (ret != ESP_OK) {
            log("Failed to apply AP configuration");
            return false;
        }

        ret = esp_wifi_set_ps(WIFI_PS_NONE);
        if (ret != ESP_OK) {
            log("Failed to disable Wi-Fi power save");
            return false;
        }

        ret = esp_wifi_start();
        if (ret != ESP_OK) {
            log("Failed to start AP");
            return false;
        }

        return true;
    }

    bool start_web_server()
    {
        httpd_config_t config    = HTTPD_DEFAULT_CONFIG();
        config.max_uri_handlers  = 16;
        config.recv_wait_timeout = 15;
        config.send_wait_timeout = 15;
        config.uri_match_fn      = httpd_uri_match_wildcard;
        // handle_upload() has a 4KB streaming buffer plus a deep synchronous
        // call chain into FATFS/wear-levelling/flash read; the 4KB default
        // stack_size overflows and corrupts adjacent heap memory.
        config.stack_size = 10240;

        esp_err_t ret = httpd_start(&_server, &config);
        if (ret != ESP_OK) {
            log("Failed to start web server");
            return false;
        }

        httpd_uri_t index  = {.uri = "/", .method = HTTP_GET, .handler = &Session::handle_index, .user_ctx = this};
        httpd_uri_t status = {
            .uri = "/status", .method = HTTP_GET, .handler = &Session::handle_status, .user_ctx = this};
        httpd_uri_t upload = {
            .uri = "/upload", .method = HTTP_POST, .handler = &Session::handle_upload, .user_ctx = this};
        httpd_uri_t close = {
            .uri = "/close", .method = HTTP_POST, .handler = &Session::handle_close, .user_ctx = this};
        httpd_uri_t captive = {
            .uri = nullptr, .method = HTTP_GET, .handler = &Session::handle_captive_portal, .user_ctx = this};

        esp_err_t reg = httpd_register_uri_handler(_server, &index);
        if (reg == ESP_OK) {
            reg = httpd_register_uri_handler(_server, &status);
        }
        if (reg == ESP_OK) {
            reg = httpd_register_uri_handler(_server, &upload);
        }
        if (reg == ESP_OK) {
            reg = httpd_register_uri_handler(_server, &close);
        }
        if (reg == ESP_OK) {
            for (const auto* url : _captive_portal_urls) {
                captive.uri = url;
                reg         = httpd_register_uri_handler(_server, &captive);
                if (reg != ESP_OK) {
                    break;
                }
            }
        }
        if (reg != ESP_OK) {
            log("Failed to register web routes");
            return false;
        }

        return true;
    }

    void stop()
    {
        if (_server != nullptr) {
            httpd_stop(_server);
            _server = nullptr;
        }
        if (_dns_server) {
            _dns_server->Stop();
            _dns_server.reset();
        }
        esp_err_t ret = esp_wifi_stop();
        if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STARTED && ret != ESP_ERR_WIFI_MODE) {
            ESP_LOGW(_tag, "wifi stop failed: %s", esp_err_to_name(ret));
        }
        if (_event_group != nullptr) {
            vEventGroupDelete(_event_group);
            _event_group = nullptr;
        }
    }

    static Session* self_from_request(httpd_req_t* req)
    {
        return static_cast<Session*>(req->user_ctx);
    }

    static bool query_flag_set(httpd_req_t* req, const char* key)
    {
        char query[128] = {};
        if (httpd_req_get_url_query_len(req) <= 0 ||
            httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
            return false;
        }
        char value[8] = {};
        return httpd_query_key_value(query, key, value, sizeof(value)) == ESP_OK && std::string(value) == "1";
    }

    static std::string query_value(httpd_req_t* req, const char* key)
    {
        char query[128] = {};
        if (httpd_req_get_url_query_len(req) <= 0 ||
            httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
            return {};
        }
        char value[64] = {};
        if (httpd_query_key_value(query, key, value, sizeof(value)) != ESP_OK) {
            return {};
        }
        return std::string(value);
    }

    static void send_json(httpd_req_t* req, const std::string& body)
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, body.c_str(), body.size());
    }

    static void send_json_error(httpd_req_t* req, const char* status, const std::string& message)
    {
        httpd_resp_set_status(req, status);
        send_json(req, "{\"error\":\"" + message + "\"}");
    }

    static esp_err_t handle_index(httpd_req_t* req)
    {
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        httpd_resp_send(req, moments_upload_html_start, moments_upload_html_end - moments_upload_html_start);
        return ESP_OK;
    }

    static esp_err_t handle_status(httpd_req_t* req)
    {
        const auto count  = moments::storage::load_existing_photos().size();
        const std::string body = "{\"photos\":" + std::to_string(count) +
                                  ",\"max_photos\":" + std::to_string(moments::storage::kMaxPhotos) +
                                  ",\"max_file_bytes\":" + std::to_string(_max_upload_bytes) + "}";
        send_json(req, body);
        return ESP_OK;
    }

    static esp_err_t handle_close(httpd_req_t* req)
    {
        auto* self = self_from_request(req);
        if (self != nullptr && self->_event_group != nullptr) {
            xEventGroupSetBits(self->_event_group, _exit_requested_bit);
        }
        httpd_resp_sendstr(req, "closing");
        return ESP_OK;
    }

    static esp_err_t handle_captive_portal(httpd_req_t* req)
    {
        const std::string url = std::string(_ap_url) + "/?_=" + std::to_string(esp_timer_get_time());
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", url.c_str());
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send(req, nullptr, 0);
        return ESP_OK;
    }

    static esp_err_t handle_upload(httpd_req_t* req)
    {
        auto* self = self_from_request(req);
        if (self == nullptr) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "server error");
            return ESP_FAIL;
        }

        char content_type[64] = {};
        httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type));
        if (std::string(content_type) != "image/bmp") {
            send_json_error(req, "415 Unsupported Media Type", "only bmp images are supported");
            return ESP_FAIL;
        }

        if (req->content_len <= 0) {
            send_json_error(req, "400 Bad Request", "empty upload");
            return ESP_FAIL;
        }
        if (static_cast<std::size_t>(req->content_len) > _max_upload_bytes) {
            send_json_error(req, "413 Payload Too Large", "file too large");
            return ESP_FAIL;
        }

        const std::string batch_token = query_value(req, "batch");
        const bool reset =
            query_flag_set(req, "reset") || batch_token.empty() || batch_token != self->_active_batch_token;
        const bool last_in_batch = query_flag_set(req, "last") || batch_token.empty();

        if (reset) {
            moments::storage::delete_all_photos();
            self->_batch_id           = GetHAL().millis();
            self->_active_batch_token = batch_token;
            self->_photos_in_batch    = 0;
        }

        if (self->_photos_in_batch >= moments::storage::kMaxPhotos) {
            send_json_error(req, "409 Conflict", "too many photos");
            return ESP_FAIL;
        }

        const long free_bytes = moments::storage::free_space_bytes();
        if (free_bytes >= 0 && free_bytes < req->content_len + 65536) {
            send_json_error(req, "507 Insufficient Storage", "not enough storage");
            return ESP_FAIL;
        }

        const std::string temp_path = moments::storage::temp_upload_path();
        FILE* file                  = fopen(temp_path.c_str(), "wb");
        if (file == nullptr) {
            send_json_error(req, "500 Internal Server Error", "failed to store image");
            return ESP_FAIL;
        }

        char buffer[4096];
        std::size_t remaining = static_cast<std::size_t>(req->content_len);
        bool write_failed     = false;
        while (remaining > 0) {
            const int to_read  = static_cast<int>(remaining < sizeof(buffer) ? remaining : sizeof(buffer));
            const int received = httpd_req_recv(req, buffer, to_read);
            if (received <= 0) {
                write_failed = true;
                break;
            }
            if (fwrite(buffer, 1, static_cast<std::size_t>(received), file) != static_cast<std::size_t>(received)) {
                write_failed = true;
                break;
            }
            remaining -= static_cast<std::size_t>(received);
        }
        fclose(file);

        if (write_failed) {
            unlink(temp_path.c_str());
            send_json_error(req, "500 Internal Server Error", "upload interrupted");
            return ESP_FAIL;
        }

        int width  = 0;
        int height = 0;
        if (!moments::storage::read_bmp_dimensions(temp_path, width, height) ||
            width != moments::storage::kPhotoWidth || height != moments::storage::kPhotoHeight) {
            unlink(temp_path.c_str());
            send_json_error(req, "400 Bad Request", "invalid preview image");
            return ESP_FAIL;
        }

        const std::string final_path = moments::storage::make_photo_path(self->_batch_id, self->_photos_in_batch);
        if (rename(temp_path.c_str(), final_path.c_str()) != 0) {
            unlink(temp_path.c_str());
            send_json_error(req, "500 Internal Server Error", "failed to finalize image");
            return ESP_FAIL;
        }
        self->_photos_in_batch += 1;

        self->log("Uploading " + std::to_string(self->_photos_in_batch) + "/" +
                   std::to_string(moments::storage::kMaxPhotos) + "...");
        if (last_in_batch) {
            self->log("Upload complete, tap Done to finish");
        }

        send_json(req, "{\"ok\":true,\"photos\":" + std::to_string(self->_photos_in_batch) + "}");
        return ESP_OK;
    }

    httpd_handle_t _server          = nullptr;
    EventGroupHandle_t _event_group = nullptr;
    std::function<void(std::string_view)> _on_log;
    std::string _ssid;
    std::unique_ptr<DnsServer> _dns_server;

    std::string _active_batch_token;
    uint32_t _batch_id           = 0;
    std::size_t _photos_in_batch = 0;
};

}  // namespace

void run_upload_mode(const std::function<void(std::string_view)>& onLog)
{
    Session(onLog).run();
}

}  // namespace moments::net
```

- [ ] **Step 4: Register the embedded HTML asset in `main/CMakeLists.txt`**

Replace:

```cmake
    EMBED_TXTFILES
        "hal/utils/config_ap/assets/badge_config_ap.html"
```

with:

```cmake
    EMBED_TXTFILES
        "apps/app_moments/assets/moments_upload.html"
```

- [ ] **Step 5: Build verification**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
. ~/esp/esp-idf/export.sh
idf.py build
```

Expected: `Project build complete.` (`moments::net::run_upload_mode` isn't
called from `AppMoments` yet — that's Task 8 — but it must compile and
link standalone, including the embedded HTML symbol.)

- [ ] **Step 6: Commit**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
git add main/apps/app_moments/assets main/apps/app_moments/net main/CMakeLists.txt
git commit -m "$(cat <<'EOF'
Add the Moments Wi-Fi AP + HTTP upload server

Ports the Python version's upload flow (client-side canvas resize to an
exact 466x466 BMP, batch upload with reset/last semantics, same HTTP
status codes) onto esp_http_server + esp_wifi AP mode, following the
same Session/DnsServer pattern the demo's Badge app used for config_ap.
EOF
)"
```

---

## Task 8: Wire upload mode end-to-end into AppMoments

**Files:**
- Modify: `main/apps/app_moments/app_moments.cpp`

**Interfaces:**
- Consumes: `moments::net::run_upload_mode` (Task 7); `view::LoadingPage` (existing `apps/common/loading_page`)

- [ ] **Step 1: Replace the upload-request log line with the real flow**

In `main/apps/app_moments/app_moments.cpp`, add includes:

```cpp
#include "net/upload_server.h"
#include <apps/common/loading_page/loading_page.h>
```

Replace:

```cpp
    if (_view->consumeUploadRequested()) {
        mclog::tagInfo(getAppInfo().name, "upload requested (upload mode not wired up yet)");
    }
}
```

with:

```cpp
    if (_view->consumeUploadRequested()) {
        mclog::tagInfo(getAppInfo().name, "start upload mode");

        auto loading_page = std::make_unique<view::LoadingPage>(0x000000, 0xFFFFFF);
        loading_page->setMessage("Starting upload mode...");
        _view.reset();

        GetHAL().lvglUnlock();
        moments::net::run_upload_mode([&](std::string_view message) {
            LvglLockGuard log_lock;
            loading_page->setMessage(message);
        });
        GetHAL().lvglLock();

        loading_page.reset();
        _view = std::make_unique<view::MomentsView>();
        _view->init(lv_screen_active());
        reloadPhotos();
    }
}
```

- [ ] **Step 2: Build verification**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
. ~/esp/esp-idf/export.sh
idf.py build
```

Expected: `Project build complete.`

- [ ] **Step 3: Commit**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
git add main/apps/app_moments/app_moments.cpp
git commit -m "$(cat <<'EOF'
Wire upload mode into AppMoments end-to-end

Confirming the upload dialog now shows a loading page, starts the AP +
HTTP server (blocking, same lvglUnlock/relock pattern the demo's Badge
app uses around its own blocking AP session), and reloads the slideshow
from /spiflash/moments once the phone-side "Done" button ends the
session.
EOF
)"
```

---

## Task 9: Final full build and docs

**Files:**
- Modify: `README.md` (if anything needs correction after the full build)

**Interfaces:** N/A.

- [ ] **Step 1: Clean full build**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
. ~/esp/esp-idf/export.sh
idf.py fullclean
idf.py build
```

Expected: `Project build complete.` — this is the final acceptance check
for the whole plan: a from-scratch build of the pruned framework plus
Moments succeeds.

- [ ] **Step 2: Sanity-check the binary size against the partition table**

```bash
idf.py size
```

Expected: no "app partition is too small" warning against the `ota_0`
partition (`0x4f0000` bytes) in `partitions.csv`.

- [ ] **Step 3: Commit any final touch-ups**

```bash
cd /Users/wenhuabin/Project/m5-stopwatch
git status
```

If nothing changed, skip the commit. Otherwise:

```bash
git add -A -- ':!components' ':!build' ':!sdkconfig' ':!sdkconfig.old'
git commit -m "$(cat <<'EOF'
Final build verification for the Moments multi-app firmware
EOF
)"
```

---

## Self-Review Notes

- **Spec coverage:** bootstrap/pruning (Tasks 1-2), storage (Task 3), app
  lifecycle + launcher integration (Task 4), touch nav + GoHome (Task 5),
  upload trigger dialog (Task 6), AP/HTTP server + HTML page (Task 7),
  end-to-end wiring (Task 8), error-handling status codes (all present in
  Task 7's `handle_upload` — 415/400/413/409/507 match the spec's table),
  build verification (Tasks 1, 2, 3, 4, 5, 6, 7, 8, 9) all map to a task.
- **Placeholder scan:** no TBD/TODO; every code step has full file
  contents, not summaries.
- **Type consistency:** `view::MomentsView`/`view::TapSide`/
  `view::MomentsUploadDialog` (Task 4) are used with the same names and
  signatures in Tasks 5, 6, 8; `moments::storage::*` (Task 3) names match
  exactly what Tasks 4, 7 call; `moments::net::run_upload_mode` (Task 7)
  signature matches its Task 8 call site.
