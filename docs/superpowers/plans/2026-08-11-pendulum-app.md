# Pendulum App Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a fourth app, "Pendulum" — an interactive damped single-pendulum simulation the
user can drag to set an angle, release, and watch swing/settle.

**Architecture:** New `main/apps/app_pendulum/` directory following the existing
`app_stopwatch` shape: `AppPendulum` (lifecycle + physics integration) and `view::PendulumView`
(rendering + touch-drag). Registered in `main/main.cpp` and `main/apps/apps.h` like the other
three apps. A new launcher icon is generated as a plain RGB565 LVGL image, same format as the
existing app icons.

**Tech Stack:** ESP-IDF v5.5.4, mooncake app framework, LVGL v9 via the project's
`uitk::lvgl_cpp` wrapper, C++ (no new dependencies).

## Global Constraints

- No automated test framework exists for this embedded/LVGL codebase — every prior app in
  this project (Moments, Stopwatch, Todo) was verified by building, flashing to the physical
  M5Stack StopWatch device at `/dev/cu.usbmodem1101`, and manually exercising it. This plan's
  "verify" steps follow that same pattern (build cleanly, then a final hardware pass) instead
  of a `pytest`-style red/green loop.
- New `.cpp`/`.c` source files are not picked up by `main/CMakeLists.txt`'s
  `file(GLOB_RECURSE ...)` until `idf.py reconfigure` is run — always reconfigure after adding
  a new file, before building.
- Source ESP-IDF before any `idf.py` command: `. ~/esp/esp-idf/export.sh`.
- Check `lsof /dev/cu.usbmodem1101` and kill any leftover listener before flashing.
- Follow existing file header convention: every new `.h`/`.cpp` starts with the
  `SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD` / `SPDX-License-Identifier: MIT`
  block (copy verbatim from `main/apps/app_stopwatch/app_stopwatch.h`).
- Commit after each task (matches this project's established per-fix commit rhythm).

---

### Task 1: Scaffold the Pendulum app and register it

**Files:**
- Create: `main/apps/app_pendulum/app_pendulum.h`
- Create: `main/apps/app_pendulum/app_pendulum.cpp`
- Create: `main/apps/app_pendulum/view/view.h`
- Create: `main/apps/app_pendulum/view/view.cpp`
- Modify: `main/apps/apps.h`
- Modify: `main/main.cpp`

**Interfaces:**
- Produces: `class view::PendulumView { void init(lv_obj_t* parent = lv_screen_active()); }` —
  just enough to build a blank white 466x466 panel with a "Pendulum" title, so Task 1 is
  independently verifiable before physics/touch are added.
- Produces: `class AppPendulum : public mooncake::AppAbility` with the standard
  `onCreate/onOpen/onRunning/onClose` lifecycle, installed in `main.cpp`.

- [ ] **Step 1: Create `main/apps/app_pendulum/view/view.h`**

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstdint>
#include <memory>
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>

namespace view {

// White panel with a bold "Pendulum" title, a fixed pivot, a rod, and a
// draggable bob. Drag near the bob to set an angle; release to let it
// swing under damped-pendulum physics driven by the owning AppAbility.
class PendulumView {
public:
    void init(lv_obj_t* parent = lv_screen_active());

    // Repositions the rod + bob for the given angle (radians, 0 = straight
    // down, positive = swung toward +x).
    void setAngle(double thetaRad);

    bool isDragging() const
    {
        return _is_dragging;
    }

    // Touch-driven angle while isDragging() is true. Only meaningful
    // while dragging.
    double dragAngleRad() const
    {
        return _drag_angle_rad;
    }

    // True exactly once, on the frame after the user releases a drag.
    bool consumeReleaseRequested();

private:
    void updateDragFromTouch(lv_event_t* e);

    static void handlePressed(lv_event_t* e);
    static void handlePressing(lv_event_t* e);
    static void handleReleased(lv_event_t* e);

    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _title_label;
    std::unique_ptr<uitk::lvgl_cpp::Container> _pivot_dot;
    std::unique_ptr<uitk::lvgl_cpp::Line> _rod;
    std::unique_ptr<uitk::lvgl_cpp::Container> _bob;

    bool _is_dragging        = false;
    double _drag_angle_rad   = 0.0;
    bool _release_requested  = false;
};

}  // namespace view
```

- [ ] **Step 2: Create `main/apps/app_pendulum/view/view.cpp`** (Task 1 version — structure
  and a fixed test angle only; drag handling and `setAngle` math land in Task 3/4)

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"

#include <assets/assets.h>

using namespace view;
using namespace uitk::lvgl_cpp;

namespace {

constexpr int _panel_size = 466;

constexpr uint32_t _color_panel_bg = 0xFFFFFF;
constexpr uint32_t _color_title    = 0x1A1A1A;

}  // namespace

void PendulumView::init(lv_obj_t* parent)
{
    _is_dragging       = false;
    _drag_angle_rad    = 0.0;
    _release_requested = false;

    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_panel_size, _panel_size);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->setBgColor(lv_color_hex(_color_panel_bg));
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _title_label = std::make_unique<Label>(_panel->get());
    _title_label->setText("Pendulum");
    _title_label->setTextFont(&TodoTitleBold40);
    _title_label->setTextColor(lv_color_hex(_color_title));
    _title_label->align(LV_ALIGN_CENTER, 0, -195);
}

bool PendulumView::consumeReleaseRequested()
{
    const bool requested = _release_requested;
    _release_requested    = false;
    return requested;
}
```

- [ ] **Step 3: Create `main/apps/app_pendulum/app_pendulum.h`**

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "view/view.h"
#include <apps/common/key_manager/key_manager.h>
#include <cstdint>
#include <memory>
#include <mooncake.h>

class AppPendulum : public mooncake::AppAbility {
public:
    AppPendulum();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    // BtnA: snap back to the default angle and restart the swing.
    void resetPendulum();
    // Advances the damped-pendulum ODE by dtSeconds (semi-implicit Euler).
    void stepPhysics(double dtSeconds);

    std::unique_ptr<input::KeyManager> _key_manager;
    std::unique_ptr<view::PendulumView> _view;

    double _theta_rad        = 0.0;
    double _omega_rad_s      = 0.0;
    uint32_t _last_update_ms = 0;
};
```

- [ ] **Step 4: Create `main/apps/app_pendulum/app_pendulum.cpp`** (Task 1 version — no
  physics/drag wiring yet, just lifecycle + a static angle, so the app opens and shows the
  panel)

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_pendulum.h"

#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>

using namespace mooncake;

AppPendulum::AppPendulum()
{
    setAppInfo().name = "Pendulum";
}

void AppPendulum::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppPendulum::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();

    LvglLockGuard lock;
    _view = std::make_unique<view::PendulumView>();
    _view->init(lv_screen_active());
}

void AppPendulum::onRunning()
{
    input::KeyEvent event = input::KeyEvent::None;
    if (_key_manager) {
        event = _key_manager->update();
    }

    if (event == input::KeyEvent::GoHome) {
        close();
        return;
    }
}

void AppPendulum::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();

    LvglLockGuard lock;
    _view.reset();
}

void AppPendulum::resetPendulum()
{
    _theta_rad   = 0.0;
    _omega_rad_s = 0.0;
}

void AppPendulum::stepPhysics(double /*dtSeconds*/)
{
    // Implemented in Task 5.
}
```

- [ ] **Step 5: Register the app** — add to `main/apps/apps.h`:

```cpp
#include "app_pendulum/app_pendulum.h"
```

(anywhere in the include list, e.g. alongside the other `app_*` includes)

And to `main/main.cpp`, in the "Install apps" section:

```cpp
    GetMooncake().installApp(std::make_unique<AppPendulum>());
```

(add this line after the existing `AppTodo` install line)

- [ ] **Step 6: Reconfigure and build**

```bash
. ~/esp/esp-idf/export.sh
idf.py reconfigure
idf.py build
```

Expected: builds with no errors (new files picked up via `reconfigure`).

- [ ] **Step 7: Commit**

```bash
git add main/apps/app_pendulum main/apps/apps.h main/main.cpp
git commit -m "Scaffold Pendulum app skeleton and register it"
```

---

### Task 2: Generate and wire up the launcher icon

**Files:**
- Create: `main/assets/images/icon_pendulum_app.c`
- Modify: `main/assets/assets.h`
- Modify: `main/apps/app_pendulum/app_pendulum.cpp`

**Interfaces:**
- Produces: `LV_IMG_DECLARE(icon_pendulum_app);` in `assets.h`, a 200x200 `LV_COLOR_FORMAT_RGB565`
  `lv_image_dsc_t` matching the exact struct shape of `icon_todo_app`/`icon_stopwatch_app`
  (`.header.cf/.header.magic/.header.w/.header.h/.data_size/.data`).

- [ ] **Step 1: Generate the icon bitmap**

The existing app icons (`icon_todo_app.c`, `icon_stopwatch_app.c`) are solid-dark-background
RGB565 bitmaps drawn on top of the launcher's black panel (confirmed by sampling their corner
pixel: `0xa3,0x10` little-endian = a near-black navy, not the app's own light foreground color).
Generate a matching icon — black background, a white pivot dot + rod, and a blue bob (the same
`0x5865F2` accent used by the app itself) swung to a 45° resting pose — with this pure-Python
script (no new dependencies; run it once from the scratchpad, then copy the output into the repo):

```python
#!/usr/bin/env python3
import math

W = H = 200
BG = (0, 0, 0)
ROD_COLOR = (230, 230, 230)
BOB_COLOR = (0x58, 0x65, 0xF2)

pivot = (100, 40)
angle = math.radians(45)
length = 110
bob_center = (pivot[0] + length * math.sin(angle), pivot[1] + length * math.cos(angle))
bob_radius = 22
rod_half_width = 3
pivot_radius = 8

def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

def dist_to_segment(px, py, ax, ay, bx, by):
    abx, aby = bx - ax, by - ay
    apx, apy = px - ax, py - ay
    ab_len_sq = abx * abx + aby * aby
    t = max(0.0, min(1.0, (apx * abx + apy * aby) / ab_len_sq))
    cx, cy = ax + t * abx, ay + t * aby
    return math.hypot(px - cx, py - cy)

pixels = [BG] * (W * H)
for y in range(H):
    for x in range(W):
        if math.hypot(x - bob_center[0], y - bob_center[1]) <= bob_radius:
            pixels[y * W + x] = BOB_COLOR
        elif dist_to_segment(x, y, pivot[0], pivot[1], bob_center[0], bob_center[1]) <= rod_half_width:
            pixels[y * W + x] = ROD_COLOR
        elif math.hypot(x - pivot[0], y - pivot[1]) <= pivot_radius:
            pixels[y * W + x] = ROD_COLOR

data_bytes = bytearray()
for r, g, b in pixels:
    v = rgb565(r, g, b)
    data_bytes += bytes([v & 0xFF, (v >> 8) & 0xFF])

lines = []
for i in range(0, len(data_bytes), 18):
    chunk = data_bytes[i:i + 18]
    lines.append('    ' + ', '.join(f'0x{b:02x}' for b in chunk) + ',')

with open('icon_pendulum_app.c', 'w') as f:
    f.write('''#ifdef __has_include
#if __has_include("lvgl.h")
#ifndef LV_LVGL_H_INCLUDE_SIMPLE
#define LV_LVGL_H_INCLUDE_SIMPLE
#endif
#endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMAGE_ICON_PENDULUM_APP
#define LV_ATTRIBUTE_IMAGE_ICON_PENDULUM_APP
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_ICON_PENDULUM_APP uint8_t icon_pendulum_app_map[] = {
''')
    f.write('\n'.join(lines))
    f.write('''
};

const lv_image_dsc_t icon_pendulum_app = {
    .header.cf    = LV_COLOR_FORMAT_RGB565,
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.w     = 200,
    .header.h     = 200,
    .data_size    = 40000 * 2,
    .data         = icon_pendulum_app_map,
};
''')

print('wrote icon_pendulum_app.c,', len(data_bytes), 'bytes of pixel data')
```

Run it (from the scratchpad directory) and copy the result into the repo:

```bash
python3 gen_pendulum_icon.py
cp icon_pendulum_app.c /Users/wenhuabin/Project/m5-stopwatch/main/assets/images/
```

- [ ] **Step 2: Declare the icon** — add to `main/assets/assets.h`:

```cpp
LV_IMG_DECLARE(icon_pendulum_app);
```

(alongside the other `LV_IMG_DECLARE(icon_*_app)` lines)

- [ ] **Step 3: Wire it into the app** — in `main/apps/app_pendulum/app_pendulum.cpp`, add the
  include and set the icon in the constructor:

```cpp
#include <assets/assets.h>
```

```cpp
AppPendulum::AppPendulum()
{
    setAppInfo().name = "Pendulum";
    setAppInfo().icon = (void*)&icon_pendulum_app;
}
```

- [ ] **Step 4: Reconfigure and build**

```bash
idf.py reconfigure
idf.py build
```

Expected: builds cleanly; the new `.c` image file is picked up automatically by
`main/CMakeLists.txt`'s `assets/*.c` glob.

- [ ] **Step 5: Commit**

```bash
git add main/assets/images/icon_pendulum_app.c main/assets/assets.h main/apps/app_pendulum/app_pendulum.cpp
git commit -m "Add Pendulum launcher icon"
```

---

### Task 3: Render the pivot, rod, and bob

**Files:**
- Modify: `main/apps/app_pendulum/view/view.h`
- Modify: `main/apps/app_pendulum/view/view.cpp`
- Modify: `main/apps/app_pendulum/app_pendulum.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `void PendulumView::setAngle(double thetaRad)` — the only entry point the app layer
  needs to move the rod/bob. Pivot position, rod length, and bob radius are private constants
  inside `view.cpp` (`_pivot_x`, `_pivot_y`, `_rod_length`, `_bob_radius`).

- [ ] **Step 1: Add layout constants and the three widgets in `view.cpp`**

Replace the `namespace { ... }` block at the top of `view.cpp` with:

```cpp
namespace {

constexpr int _panel_size = 466;

constexpr double _pivot_x     = 233.0;
constexpr double _pivot_y     = 165.0;
constexpr double _rod_length  = 150.0;
constexpr int _bob_radius     = 24;
constexpr int _pivot_dot_size = 12;
constexpr int _rod_width      = 4;

constexpr uint32_t _color_panel_bg = 0xFFFFFF;
constexpr uint32_t _color_title    = 0x1A1A1A;
constexpr uint32_t _color_pivot    = 0x1A1A1A;
constexpr uint32_t _color_rod      = 0x4A4A4A;
constexpr uint32_t _color_bob      = 0x5865F2;

}  // namespace
```

Append to `PendulumView::init`, after the `_title_label` block:

```cpp
    _pivot_dot = std::make_unique<Container>(_panel->get());
    _pivot_dot->setSize(_pivot_dot_size, _pivot_dot_size);
    _pivot_dot->setRadius(LV_RADIUS_CIRCLE);
    _pivot_dot->setBorderWidth(0);
    _pivot_dot->setBgColor(lv_color_hex(_color_pivot));
    _pivot_dot->setBgOpa(LV_OPA_COVER);
    _pivot_dot->setPos(static_cast<int32_t>(_pivot_x) - _pivot_dot_size / 2,
                        static_cast<int32_t>(_pivot_y) - _pivot_dot_size / 2);

    _rod = std::make_unique<Line>(_panel->get());
    _rod->setLineColor(lv_color_hex(_color_rod));
    _rod->setLineWidth(_rod_width);
    _rod->setLineRounded(true);

    _bob = std::make_unique<Container>(_panel->get());
    _bob->setSize(_bob_radius * 2, _bob_radius * 2);
    _bob->setRadius(LV_RADIUS_CIRCLE);
    _bob->setBorderWidth(0);
    _bob->setBgColor(lv_color_hex(_color_bob));
    _bob->setBgOpa(LV_OPA_COVER);
```

- [ ] **Step 2: Implement `setAngle`** — add to `view.cpp` (needs `#include <cmath>` added at
  the top of the file):

```cpp
void PendulumView::setAngle(double thetaRad)
{
    const double bob_x = _pivot_x + _rod_length * std::sin(thetaRad);
    const double bob_y = _pivot_y + _rod_length * std::cos(thetaRad);

    const lv_point_precise_t points[2] = {
        {static_cast<lv_value_precise_t>(_pivot_x), static_cast<lv_value_precise_t>(_pivot_y)},
        {static_cast<lv_value_precise_t>(bob_x), static_cast<lv_value_precise_t>(bob_y)},
    };
    _rod->setPoints(points, 2);

    _bob->setPos(static_cast<int32_t>(bob_x) - _bob_radius, static_cast<int32_t>(bob_y) - _bob_radius);
}
```

- [ ] **Step 3: Call it once from the app** — in `app_pendulum.cpp`'s `onOpen()`, after
  `_view->init(lv_screen_active());`, add:

```cpp
    _theta_rad      = 45.0 * 3.14159265358979323846 / 180.0;
    _last_update_ms = GetHAL().millis();
    _view->setAngle(_theta_rad);
```

- [ ] **Step 4: Reconfigure and build**

```bash
idf.py reconfigure
idf.py build
```

Expected: builds cleanly.

- [ ] **Step 5: Flash and visually verify static rendering**

```bash
lsof /dev/cu.usbmodem1101
idf.py -p /dev/cu.usbmodem1101 flash
```

Open the Pendulum app from the launcher. Expected: white panel, "Pendulum" title, a dark pivot
dot near the top, a gray rod, and a blue bob resting at a fixed 45° angle (no swinging yet —
that's Task 5).

- [ ] **Step 6: Commit**

```bash
git add main/apps/app_pendulum
git commit -m "Render pendulum pivot, rod, and bob at a fixed angle"
```

---

### Task 4: Touch-drag interaction

**Files:**
- Modify: `main/apps/app_pendulum/view/view.cpp`

**Interfaces:**
- Consumes: `_pivot_x`, `_pivot_y` from Task 3.
- Produces: `isDragging()`, `dragAngleRad()`, `consumeReleaseRequested()` (already declared in
  `view.h` from Task 1) become live — this task fills in their implementation.

- [ ] **Step 1: Add the max-drag-angle constant** — in the `namespace { ... }` block in
  `view.cpp`:

```cpp
constexpr double _max_drag_angle_rad = 80.0 * 3.14159265358979323846 / 180.0;
```

- [ ] **Step 2: Register touch handlers** — in `PendulumView::init`, after the `_panel` styling
  calls and before `_title_label` is created:

```cpp
    _panel->onPressed(handlePressed, this);
    _panel->addEventCb(handlePressing, LV_EVENT_PRESSING, this);
    _panel->onRelease(handleReleased, this);
```

- [ ] **Step 3: Implement the drag handlers** — add to `view.cpp`:

```cpp
void PendulumView::updateDragFromTouch(lv_event_t* e)
{
    lv_indev_t* indev = lv_event_get_indev(e);
    if (indev == nullptr) {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    const double dx = point.x - _pivot_x;
    const double dy = point.y - _pivot_y;
    if (dx == 0.0 && dy == 0.0) {
        return;
    }

    double angle = std::atan2(dx, dy);
    if (angle > _max_drag_angle_rad) {
        angle = _max_drag_angle_rad;
    } else if (angle < -_max_drag_angle_rad) {
        angle = -_max_drag_angle_rad;
    }
    _drag_angle_rad = angle;
}

void PendulumView::handlePressed(lv_event_t* e)
{
    auto* self = static_cast<PendulumView*>(lv_event_get_user_data(e));
    if (self == nullptr) {
        return;
    }
    self->_is_dragging = true;
    self->updateDragFromTouch(e);
}

void PendulumView::handlePressing(lv_event_t* e)
{
    auto* self = static_cast<PendulumView*>(lv_event_get_user_data(e));
    if (self == nullptr || !self->_is_dragging) {
        return;
    }
    self->updateDragFromTouch(e);
}

void PendulumView::handleReleased(lv_event_t* e)
{
    auto* self = static_cast<PendulumView*>(lv_event_get_user_data(e));
    if (self == nullptr) {
        return;
    }
    self->_is_dragging       = false;
    self->_release_requested = true;
}
```

- [ ] **Step 4: Reconfigure and build**

```bash
idf.py reconfigure
idf.py build
```

Expected: builds cleanly. (No visible behavior change yet — the app layer doesn't read
`isDragging()`/`dragAngleRad()` until Task 5, so the bob still looks static on hardware right
now. That's expected and gets verified end-to-end in Task 5's hardware pass.)

- [ ] **Step 5: Commit**

```bash
git add main/apps/app_pendulum/view/view.cpp
git commit -m "Add touch-drag angle tracking to PendulumView"
```

---

### Task 5: Physics integration, BtnA reset, and full hardware verification

**Files:**
- Modify: `main/apps/app_pendulum/app_pendulum.cpp`

**Interfaces:**
- Consumes: `PendulumView::isDragging()`, `dragAngleRad()`, `consumeReleaseRequested()`,
  `setAngle(double)` — all already implemented (Tasks 1, 3, 4).

- [ ] **Step 1: Add physics constants** — in the `namespace { ... }` block at the top of
  `app_pendulum.cpp` (create it if not already present, right after the includes):

```cpp
namespace {
constexpr double kOmega0Sq        = 6.8;   // rad^2/s^2 -- tuned for a ~2.4s period
constexpr double kDamping         = 0.15;  // 1/s -- tuned for a ~15-20s settle time
constexpr double kDefaultAngleRad = 45.0 * 3.14159265358979323846 / 180.0;
constexpr uint32_t kMaxDtMs       = 50;
}  // namespace
```

Remove the now-redundant inline `45.0 * 3.14159265358979323846 / 180.0` literal added in Task 3
Step 3 and replace it with `kDefaultAngleRad`.

- [ ] **Step 2: Implement `resetPendulum` and `stepPhysics`** — replace the Task 1 stub bodies:

```cpp
void AppPendulum::resetPendulum()
{
    _theta_rad   = kDefaultAngleRad;
    _omega_rad_s = 0.0;
}

void AppPendulum::stepPhysics(double dtSeconds)
{
    const double angular_accel = -kOmega0Sq * std::sin(_theta_rad) - kDamping * _omega_rad_s;
    _omega_rad_s += angular_accel * dtSeconds;
    _theta_rad += _omega_rad_s * dtSeconds;
}
```

Add `#include <cmath>` to the top of `app_pendulum.cpp`.

- [ ] **Step 3: Wire physics + drag + BtnA into `onRunning`** — replace the body of
  `AppPendulum::onRunning()`:

```cpp
void AppPendulum::onRunning()
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

    if (event == input::KeyEvent::GoPrevious) {
        resetPendulum();
    }

    const uint32_t now = GetHAL().millis();
    uint32_t dt_ms      = now - _last_update_ms;
    _last_update_ms      = now;
    if (dt_ms > kMaxDtMs) {
        dt_ms = kMaxDtMs;
    }

    if (_view->isDragging()) {
        _theta_rad   = _view->dragAngleRad();
        _omega_rad_s = 0.0;
    } else {
        if (_view->consumeReleaseRequested()) {
            _omega_rad_s = 0.0;
        }
        stepPhysics(dt_ms / 1000.0);
    }

    _view->setAngle(_theta_rad);
}
```

- [ ] **Step 4: Reconfigure and build**

```bash
idf.py reconfigure
idf.py build
```

Expected: builds cleanly.

- [ ] **Step 5: Flash and verify the full interactive loop on hardware**

```bash
lsof /dev/cu.usbmodem1101
idf.py -p /dev/cu.usbmodem1101 flash
```

Manually verify on the device:
- Opening the app shows the bob resting at 45°.
- Dragging the bob tracks the touch angle and clamps around ±80° at the extremes.
- Releasing lets the pendulum swing freely and damp to rest over roughly 15-20 seconds.
- Re-dragging works both mid-swing and once it has settled.
- BtnA resets to 45° and restarts the swing from a standstill.
- Holding both buttons returns to the launcher.

If the swing feels too fast/slow/floaty, adjust `kOmega0Sq`/`kDamping` in
`app_pendulum.cpp` and reflash — this tuning pass is expected, same as Stopwatch's layout was
iterated after first flash.

- [ ] **Step 6: Commit**

```bash
git add main/apps/app_pendulum/app_pendulum.cpp
git commit -m "Wire up damped-pendulum physics, drag/release, and BtnA reset"
```
