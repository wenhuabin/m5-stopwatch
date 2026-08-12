# Pendulum Clock Face Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the Pendulum app into a pendulum clock: an analog dial (hour/minute/second
hands showing the real time) above the existing interactive pendulum, sharing one panel.

**Architecture:** New `view::ClockFace` class (`main/apps/app_pendulum/view/clock_face.h/.cpp`)
owns the dial/numerals/ticks/hands and is driven by `PendulumView::setTime()`. The pendulum's
pivot/rod/bob shrink and move down to make room. `AppPendulum::onRunning()` reads
`GetHAL().getTimeHms()` every tick and forwards it alongside the existing `setAngle()` call.

## Global Constraints

- No automated test framework for this embedded/LVGL codebase — verification is build-clean
  + flash-and-manually-check on the physical device at `/dev/cu.usbmodem1101`, same as every
  prior app/feature in this project.
- Source ESP-IDF before any `idf.py` command: `. ~/esp/esp-idf/export.sh`.
- New `.cpp` files need `idf.py reconfigure` before the next build (CMake's `GLOB_RECURSE`
  caches its file list at configure time).
- Check `lsof /dev/cu.usbmodem1101` and kill any leftover listener before flashing.
- New files start with the standard SPDX header block (copy from `main/apps/app_pendulum/view/view.h`).
- Rotated `Container` rectangles are used for hands (not `lv_line`) — this project already hit
  three separate bugs with `lv_line` on the pendulum rod (mis-sized bounding box, a dangling
  points pointer, and visible flicker) before replacing it with a rotated-rectangle technique
  that works reliably. Reuse that same technique for the clock hands.
- LVGL's positive `transform_rotation` is clockwise on screen (confirmed while fixing the
  pendulum rod's swing direction) — clock hands need no sign negation, unlike the pendulum
  rod's angle convention which does.
- Commit after each task.

---

### Task 1: Build the clock face (dial, numerals, ticks, hands) at a fixed test time

**Files:**
- Create: `main/apps/app_pendulum/view/clock_face.h`
- Create: `main/apps/app_pendulum/view/clock_face.cpp`
- Modify: `main/apps/app_pendulum/view/view.h`
- Modify: `main/apps/app_pendulum/view/view.cpp`

**Interfaces:**
- Produces: `class view::ClockFace { void init(lv_obj_t* parent); void setTime(uint8_t hour, uint8_t minute, uint8_t second); }`
- Consumes (Task 1 only): nothing external — `PendulumView::init()` calls
  `_clock_face->setTime(10, 10, 30)` once with a fixed test time so Task 1 is visually
  verifiable before live time is wired up in Task 3.

- [ ] **Step 1: Create `main/apps/app_pendulum/view/clock_face.h`**

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
#include <vector>

namespace view {

// A read-only analog clock face: 12 numerals, tick marks, and three hands
// showing the current wall-clock time. Purely decorative -- no touch
// handling of its own; the pendulum below it still owns all drag input.
class ClockFace {
public:
    void init(lv_obj_t* parent);

    void setTime(uint8_t hour, uint8_t minute, uint8_t second);

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _face;
    std::unique_ptr<uitk::lvgl_cpp::Container> _pivot_dot;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Label>> _numeral_labels;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Container>> _tick_marks;
    std::unique_ptr<uitk::lvgl_cpp::Container> _hour_hand;
    std::unique_ptr<uitk::lvgl_cpp::Container> _minute_hand;
    std::unique_ptr<uitk::lvgl_cpp::Container> _second_hand;
};

}  // namespace view
```

- [ ] **Step 2: Create `main/apps/app_pendulum/view/clock_face.cpp`**

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "clock_face.h"

#include <cmath>
#include <string>

using namespace view;
using namespace uitk::lvgl_cpp;

namespace {

constexpr double _center_x   = 233.0;
constexpr double _center_y   = 100.0;
constexpr int _face_radius   = 90;
constexpr int _numeral_radius     = 72;
constexpr int _tick_inner_radius  = 78;
constexpr int _tick_outer_radius  = 88;
constexpr int _pivot_dot_size     = 8;
constexpr int _tick_dot_size      = 4;

constexpr uint32_t _color_face_bg    = 0xFFFFFF;
constexpr uint32_t _color_border     = 0x1A1A1A;
constexpr uint32_t _color_numeral    = 0x1A1A1A;
constexpr uint32_t _color_tick       = 0x9E9E9E;
constexpr uint32_t _color_hour_hand   = 0x1A1A1A;
constexpr uint32_t _color_minute_hand = 0x1A1A1A;
constexpr uint32_t _color_second_hand = 0x5865F2;

constexpr int _hour_hand_length   = 35;
constexpr int _hour_hand_width    = 5;
constexpr int _minute_hand_length = 55;
constexpr int _minute_hand_width  = 4;
constexpr int _second_hand_length = 65;
constexpr int _second_hand_width  = 2;

constexpr double kPi = 3.14159265358979323846;

// Builds a hand as a thin rectangle pointing straight up (12 o'clock) from
// the face center, pivoting around its own bottom-center so setRotation()
// sweeps it like a real clock hand.
std::unique_ptr<Container> makeHand(lv_obj_t* parent, int length, int width, uint32_t color)
{
    auto hand = std::make_unique<Container>(parent);
    hand->setSize(width, length);
    hand->setPos(static_cast<int32_t>(_center_x) - width / 2, static_cast<int32_t>(_center_y) - length);
    hand->setRadius(width / 2);
    hand->setBorderWidth(0);
    hand->setBgColor(lv_color_hex(color));
    hand->setBgOpa(LV_OPA_COVER);
    hand->setTransformPivot(LV_PCT(50), LV_PCT(100));
    return hand;
}

}  // namespace

void ClockFace::init(lv_obj_t* parent)
{
    _face = std::make_unique<Container>(parent);
    _face->setSize(_face_radius * 2, _face_radius * 2);
    _face->setPos(static_cast<int32_t>(_center_x) - _face_radius, static_cast<int32_t>(_center_y) - _face_radius);
    _face->setRadius(LV_RADIUS_CIRCLE);
    _face->setBorderWidth(2);
    _face->setBorderColor(lv_color_hex(_color_border));
    _face->setBgColor(lv_color_hex(_color_face_bg));
    _face->setBgOpa(LV_OPA_COVER);
    _face->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _face->removeFlag(LV_OBJ_FLAG_CLICKABLE);

    for (int h = 0; h < 12; ++h) {
        const double angle = h * 30.0 * kPi / 180.0;

        auto tick = std::make_unique<Container>(parent);
        tick->setSize(_tick_dot_size, _tick_dot_size);
        tick->setRadius(LV_RADIUS_CIRCLE);
        tick->setBorderWidth(0);
        tick->setBgColor(lv_color_hex(_color_tick));
        tick->setBgOpa(LV_OPA_COVER);
        const double tick_x = _center_x + _tick_outer_radius * std::sin(angle);
        const double tick_y = _center_y - _tick_outer_radius * std::cos(angle);
        tick->setPos(static_cast<int32_t>(tick_x) - _tick_dot_size / 2,
                     static_cast<int32_t>(tick_y) - _tick_dot_size / 2);
        _tick_marks.push_back(std::move(tick));

        const int numeral = (h == 0) ? 12 : h;
        auto label = std::make_unique<Label>(parent);
        label->setText(std::to_string(numeral));
        label->setTextFont(&lv_font_montserrat_14);
        label->setTextColor(lv_color_hex(_color_numeral));
        const double num_x = _center_x + _numeral_radius * std::sin(angle);
        const double num_y = _center_y - _numeral_radius * std::cos(angle);
        label->align(LV_ALIGN_CENTER, static_cast<int32_t>(num_x - _center_x),
                     static_cast<int32_t>(num_y - 233.0));
        _numeral_labels.push_back(std::move(label));
    }

    _pivot_dot = std::make_unique<Container>(parent);
    _pivot_dot->setSize(_pivot_dot_size, _pivot_dot_size);
    _pivot_dot->setRadius(LV_RADIUS_CIRCLE);
    _pivot_dot->setBorderWidth(0);
    _pivot_dot->setBgColor(lv_color_hex(_color_border));
    _pivot_dot->setBgOpa(LV_OPA_COVER);
    _pivot_dot->setPos(static_cast<int32_t>(_center_x) - _pivot_dot_size / 2,
                        static_cast<int32_t>(_center_y) - _pivot_dot_size / 2);

    _hour_hand   = makeHand(parent, _hour_hand_length, _hour_hand_width, _color_hour_hand);
    _minute_hand = makeHand(parent, _minute_hand_length, _minute_hand_width, _color_minute_hand);
    _second_hand = makeHand(parent, _second_hand_length, _second_hand_width, _color_second_hand);
}

void ClockFace::setTime(uint8_t hour, uint8_t minute, uint8_t second)
{
    const double second_deg = second * 6.0;
    const double minute_deg = minute * 6.0 + second * 0.1;
    const double hour_deg   = (hour % 12) * 30.0 + minute * 0.5;

    _second_hand->setRotation(static_cast<int32_t>(second_deg * 10.0));
    _minute_hand->setRotation(static_cast<int32_t>(minute_deg * 10.0));
    _hour_hand->setRotation(static_cast<int32_t>(hour_deg * 10.0));
}
```

> Note on the numeral `align()` call: `label->align(LV_ALIGN_CENTER, dx, dy)` positions the
> label relative to its **parent's** center. The parent passed into `ClockFace::init()` is the
> same panel `PendulumView` uses for the rod/bob (466x466, so its center is `(233,233)`) — not
> the clock's own center `(233,100)`. `dx = num_x - _center_x` works directly since both
> centers share `x=233`. `dy` needs the offset from the **panel's** center (`233`), which is
> why it's written as `num_y - 233.0` rather than `num_y - _center_y`.

- [ ] **Step 3: Wire `ClockFace` into `PendulumView`** — add to `main/apps/app_pendulum/view/view.h`:

```cpp
#include "clock_face.h"
```

(add near the top, alongside the other includes)

Add a public method and a member:

```cpp
    // Forwards the current time to the clock face; no-op if called before init().
    void setTime(uint8_t hour, uint8_t minute, uint8_t second);
```

(add to the `public:` section, after `setAngle`)

```cpp
    std::unique_ptr<ClockFace> _clock_face;
```

(add to the `private:` section, alongside `_panel`)

- [ ] **Step 4: Construct it and add the passthrough in `main/apps/app_pendulum/view/view.cpp`** —
  in `PendulumView::init()`, right after the `_panel->onRelease(...)` block:

```cpp
    _clock_face = std::make_unique<ClockFace>();
    _clock_face->init(_panel->get());
    _clock_face->setTime(10, 10, 30);  // fixed test time, replaced with live time in Task 3
```

Add the passthrough method at the end of the file:

```cpp
void PendulumView::setTime(uint8_t hour, uint8_t minute, uint8_t second)
{
    if (_clock_face) {
        _clock_face->setTime(hour, minute, second);
    }
}
```

- [ ] **Step 5: Reconfigure and build**

```bash
. ~/esp/esp-idf/export.sh
idf.py reconfigure
idf.py build
```

Expected: builds cleanly (new `clock_face.cpp` picked up via `reconfigure`).

- [ ] **Step 6: Flash and visually verify**

```bash
lsof /dev/cu.usbmodem1101
idf.py -p /dev/cu.usbmodem1101 flash
```

Open the Pendulum app. Expected: a white circular dial in the upper portion of the panel,
numerals 1-12 around the rim, small gray tick dots, and three hands frozen showing 10:10:30
(hour hand between 10 and 11, minute hand at the "2" position, second hand at the "6"
position). The pendulum below is unchanged from before this task (still full-size — that's
Task 2).

- [ ] **Step 7: Commit**

```bash
git add main/apps/app_pendulum/view
git commit -m "Add ClockFace with dial, numerals, ticks, and hands at a fixed test time"
```

---

### Task 2: Shrink and reposition the pendulum to make room below the clock

**Files:**
- Modify: `main/apps/app_pendulum/view/view.cpp`

**Interfaces:**
- Consumes: nothing new. Pure constant/geometry changes to the existing pendulum widgets.

- [ ] **Step 1: Update the pendulum's layout constants** — in `view.cpp`'s anonymous namespace,
  change three values (`_pivot_x`, `_pivot_dot_size`, and `_rod_width` stay the same):
  - `_pivot_y`: `100.0` -> `240.0`
  - `_rod_length`: `210.0` -> `150.0`
  - `_bob_radius`: `24` -> `20`

Resulting block:

```cpp
constexpr double _pivot_x     = 233.0;
constexpr double _pivot_y     = 240.0;
constexpr double _rod_length  = 150.0;
constexpr int _bob_radius     = 20;
constexpr int _pivot_dot_size = 12;
constexpr int _rod_width      = 4;
```

`_max_drag_angle_rad` stays `70.0 * kPi / 180.0` unchanged — already re-verified against the
round bezel at the new geometry (worst case 172.5px from panel center vs a 233px bezel radius,
60px to spare).

- [ ] **Step 2: Reconfigure and build**

```bash
idf.py reconfigure
idf.py build
```

Expected: builds cleanly (constants-only change, no new files).

- [ ] **Step 3: Flash and verify on hardware**

```bash
lsof /dev/cu.usbmodem1101
idf.py -p /dev/cu.usbmodem1101 flash
```

Open the Pendulum app. Expected: the clock face (from Task 1, still at the fixed 10:10:30
test time) sits in the upper ~40% of the panel; the pendulum now sits entirely below it,
smaller than before but still fully visible with a gap between the clock's bottom edge and
the pendulum's pivot. Verify the pendulum still drags/releases/swings/damps/resets (BtnA)
correctly at the new smaller size.

- [ ] **Step 4: Commit**

```bash
git add main/apps/app_pendulum/view/view.cpp
git commit -m "Shrink and reposition the pendulum to make room for the clock face"
```

---

### Task 3: Wire up live time

**Files:**
- Modify: `main/apps/app_pendulum/app_pendulum.cpp`

**Interfaces:**
- Consumes: `PendulumView::setTime(uint8_t, uint8_t, uint8_t)` (Task 1), `TimeHms` /
  `GetHAL().getTimeHms()` (existing HAL API, already used by `main/hal/hal_rtc.cpp`).

- [ ] **Step 1: Read and forward the real time every tick** — in
  `main/apps/app_pendulum/app_pendulum.cpp`'s `onRunning()`, right after the existing
  `_view->setAngle(_theta_rad);` line:

```cpp
    const TimeHms time = GetHAL().getTimeHms();
    _view->setTime(time.hour, time.minute, time.second);
```

This runs every `onRunning()` tick, alongside the pendulum's own per-frame update — no
separate timer, matching the pendulum's already-proven unthrottled update pattern (see
Global Constraints).

- [ ] **Step 2: Remove the Task 1 fixed test time** — in
  `main/apps/app_pendulum/view/view.cpp`'s `PendulumView::init()`, delete this line (the real
  time from Task 3 Step 1 supersedes it immediately after `onOpen()` runs, but leaving a stale
  fixed-time call in `init()` would just be dead/misleading code):

```cpp
    _clock_face->setTime(10, 10, 30);  // fixed test time, replaced with live time in Task 3
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

Open the Pendulum app. Expected: the clock's hour and minute hands match the actual current
time (compare against another clock/phone), and the second hand visibly ticks forward once
per second. Leave the app open for a minute and confirm the minute hand advances correctly at
the minute boundary. Re-verify the pendulum (drag/release/swing/BtnA reset) still works
unaffected by the added time-reading call.

- [ ] **Step 5: Commit**

```bash
git add main/apps/app_pendulum
git commit -m "Wire up live time for the pendulum clock face"
```
