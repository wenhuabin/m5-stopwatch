# Pendulum Clock Face Design

## Goal

Turn the Pendulum app into a "real" pendulum clock: an analog clock face (hour,
minute, second hands, showing the actual current time) above the existing
interactive pendulum simulation, both sharing the round 466x466 panel.

## Non-goals

- No time-setting UI in this app — it reads the existing RTC-backed clock
  (`GetHAL().getTimeHms()`), which is set elsewhere in the firmware.
- No date display, alarms, or chimes — just hour/minute/second hands.
- The pendulum's physics/interaction model is unchanged (same damped ODE,
  drag-to-set-angle, BtnA reset) — only its position and size change.

## Architecture

New file `main/apps/app_pendulum/view/clock_face.h` / `.cpp`: a self-contained
`view::ClockFace` class that owns the dial, the 12 numerals, the tick marks,
and the three hands. It exposes:

```cpp
class ClockFace {
public:
    void init(lv_obj_t* parent);
    void setTime(uint8_t hour, uint8_t minute, uint8_t second);
private:
    ...
};
```

`PendulumView` gains a `std::unique_ptr<view::ClockFace> _clock_face` member,
constructs it in `init()`, and exposes a new `setTime(hour, minute, second)`
passthrough. `AppPendulum::onRunning()` reads `GetHAL().getTimeHms()` every
tick (a cheap 3-byte struct copy) and forwards it to the view alongside the
existing `setAngle()` call — no new timer/throttle infrastructure, matching
the pendulum's own already-proven unthrottled per-frame update pattern.

This keeps `PendulumView` from growing into a monolith (it already owns the
pendulum's pivot/rod/bob and drag handling) while the two visual pieces still
share one panel and one `onRunning()` tick.

## Layout

Round 466x466 panel, split roughly 40/60 top-to-bottom:

- **Clock face**: circle centered at `(233, 100)`, radius `90`. Verified
  against the round bezel: the topmost point of this circle is at distance
  `|233 - 100 + 90| = 223` from the panel's own center `(233,233)`, safely
  under the 233 bezel radius; the side/bottom points are even further inside.
  White fill (matches the panel), a dark `2px` border ring, numerals `1-12`
  in `lv_font_montserrat_14` positioned via trig at radius `72` from center,
  short tick marks at radius `78-88` for the other positions, and a small
  dark pivot dot at the center.
- **Pendulum**: pivot moves to `(233, 240)` (leaving a gap below the clock
  face's bottom edge at y=190), rod length shortens `210 -> 150`, bob radius
  shrinks `24 -> 20`. Re-verified against the round bezel at the same +/-70
  degree max drag angle: worst case (theta=70) puts the bob's edge `172.5px`
  from the panel center, well inside the `233px` bezel radius (60px to
  spare) — no further angle reduction needed this time, unlike the earlier
  rod-lengthening change which was right at the limit.

## Hands

Reusing the pendulum rod's proven technique (a thin `Container` rotated
around an explicit `setTransformPivot()`, not `lv_line`) rather than
introducing a different widget approach for a very similar visual element:

- **Hour hand**: length `35`, width `5`, dark (`0x1A1A1A`).
- **Minute hand**: length `55`, width `4`, dark (`0x1A1A1A`).
- **Second hand**: length `65`, width `2`, accent blue (`0x5865F2` — the same
  accent already used for the pendulum bob and dialog buttons elsewhere in
  this app/project).

Each hand is built pointing straight up (12 o'clock) in its unrotated state,
with its transform pivot at its own bottom-center — the opposite convention
from the pendulum rod (which points down from a top-center pivot), since
clock hands radiate upward from the center outward to each numeral.

Angle formulas (0 deg = 12 o'clock, sweeping clockwise for positive degrees):

```
second_deg = second * 6.0
minute_deg = minute * 6.0 + second * 0.1
hour_deg   = (hour % 12) * 30.0 + minute * 0.5
```

Rotation sign: LVGL's positive `transform_rotation` is clockwise on screen
(confirmed empirically while fixing the pendulum rod's direction earlier),
which is exactly the direction clock hands sweep — no negation needed here,
unlike the pendulum rod's angle convention.

## Implementation Notes / Edge Cases

- Numeral labels are static text set once in `init()` (they never change) —
  only the three hands' rotations update per frame.
- `setTime()` is a no-op-safe passthrough: if called before `init()`,
  `ClockFace`'s widgets simply don't exist yet, so `PendulumView::setTime()`
  should guard on `_clock_face` being non-null (mirrors how other views in
  this project guard optional/lazily-created widgets).
- The clock face's own numerals/ticks are pure decoration with no touch
  handling — the existing drag-to-set-pendulum-angle interaction is
  untouched and still only registered on the panel's pendulum region.
