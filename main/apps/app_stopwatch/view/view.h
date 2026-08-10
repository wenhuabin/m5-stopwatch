/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>

namespace view {

enum class StopwatchPhase {
    Idle,
    Running,
    Paused,
};

// Recreates the layout of the referenced VolosStopwatch SquareLine UI on
// our 466x466 round display: big elapsed-time readout, a three-dot
// start/stop/reset state indicator, and a lap list with current/best/worst
// lap stats. Brightness/sound/battery panels from the reference are
// intentionally left out (out of scope; brightness is a global setting).
class StopwatchView {
public:
    void init(lv_obj_t* parent = lv_screen_active());

    // Updates the big MM:SS + centiseconds readout.
    void setElapsed(uint32_t elapsedMs);

    // Highlights which of the three state dots (start / stop / reset) is
    // the next actionable one.
    void setPhase(StopwatchPhase phase);

    // Appends one "NN   mm:ss.cc" line to the lap list, updates the
    // current-lap number, and refreshes the best/worst labels if this lap
    // set a new record.
    void addLap(std::size_t lapNumber, uint32_t lapMs, bool isBest, bool isWorst);

    // Clears the lap list and resets current/best/worst back to defaults.
    void resetLaps();

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;

    std::unique_ptr<uitk::lvgl_cpp::Container> _header_panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _m5stack_label;
    std::unique_ptr<uitk::lvgl_cpp::Container> _divider;

    std::unique_ptr<uitk::lvgl_cpp::Container> _time_bg_panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _time_label;
    std::unique_ptr<uitk::lvgl_cpp::Label> _centis_label;
    std::unique_ptr<uitk::lvgl_cpp::Container> _title_bg_panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _title_label;
    std::unique_ptr<uitk::lvgl_cpp::Label> _minutes_caption;
    std::unique_ptr<uitk::lvgl_cpp::Label> _seconds_caption;
    std::unique_ptr<uitk::lvgl_cpp::Label> _centis_caption;

    std::unique_ptr<uitk::lvgl_cpp::Container> _state_dot_start;
    std::unique_ptr<uitk::lvgl_cpp::Container> _state_dot_stop;
    std::unique_ptr<uitk::lvgl_cpp::Container> _state_dot_reset;
    std::unique_ptr<uitk::lvgl_cpp::Label> _lap_save_hint;
    std::unique_ptr<uitk::lvgl_cpp::Label> _start_stop_hint;
    std::unique_ptr<uitk::lvgl_cpp::Label> _reset_start_stop_hint;

    std::unique_ptr<uitk::lvgl_cpp::Container> _chip_badge_panel;
    std::unique_ptr<uitk::lvgl_cpp::Container> _chip_badge_inner;
    std::unique_ptr<uitk::lvgl_cpp::Label> _chip_badge_label_1;
    std::unique_ptr<uitk::lvgl_cpp::Label> _chip_badge_label_2;

    std::unique_ptr<uitk::lvgl_cpp::Container> _stats_bg_panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _current_lap_caption;
    std::unique_ptr<uitk::lvgl_cpp::Label> _current_lap_value;
    std::unique_ptr<uitk::lvgl_cpp::Label> _best_lap_caption;
    std::unique_ptr<uitk::lvgl_cpp::Label> _best_lap_value;
    std::unique_ptr<uitk::lvgl_cpp::Label> _worst_lap_caption;
    std::unique_ptr<uitk::lvgl_cpp::Label> _worst_lap_value;

    std::unique_ptr<uitk::lvgl_cpp::Label> _laps_caption;
    std::unique_ptr<uitk::lvgl_cpp::TextArea> _laps_area;
};

}  // namespace view
