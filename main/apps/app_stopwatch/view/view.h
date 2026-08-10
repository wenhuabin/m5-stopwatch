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

// Vertical-stack layout: battery + title header, a RESET/START/STOP state
// row, a full-width time readout, a one-line lap-stats row, and a
// centered lap list underneath.
class StopwatchView {
public:
    void init(lv_obj_t* parent = lv_screen_active());

    // Updates the big MM:SS + centiseconds readout.
    void setElapsed(uint32_t elapsedMs);

    // Highlights which of RESET / START / STOP is the next actionable
    // word in the state row.
    void setPhase(StopwatchPhase phase);

    // Appends one "NN   mm:ss.cc" line to the lap list, updates the
    // current-lap number, and refreshes the best/worst labels if this lap
    // set a new record.
    void addLap(std::size_t lapNumber, uint32_t lapMs, bool isBest, bool isWorst);

    // Clears the lap list and resets current/best/worst back to defaults.
    void resetLaps();

    // Updates the top-of-screen battery indicator.
    void setBatteryLevel(uint8_t percent, bool charging);

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;

    std::unique_ptr<uitk::lvgl_cpp::Bar> _battery_bar;
    std::unique_ptr<uitk::lvgl_cpp::Container> _battery_nub;
    std::unique_ptr<uitk::lvgl_cpp::Image> _battery_charge_icon;
    std::unique_ptr<uitk::lvgl_cpp::Label> _battery_label;

    std::unique_ptr<uitk::lvgl_cpp::Label> _title_label;

    std::unique_ptr<uitk::lvgl_cpp::Label> _state_label_reset;
    std::unique_ptr<uitk::lvgl_cpp::Label> _state_label_start;
    std::unique_ptr<uitk::lvgl_cpp::Label> _state_label_stop;

    std::unique_ptr<uitk::lvgl_cpp::Container> _time_bg_panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _time_label;
    std::unique_ptr<uitk::lvgl_cpp::Label> _centis_label;

    std::unique_ptr<uitk::lvgl_cpp::Label> _current_lap_label;
    std::unique_ptr<uitk::lvgl_cpp::Label> _best_lap_label;
    std::unique_ptr<uitk::lvgl_cpp::Label> _worst_lap_label;

    std::unique_ptr<uitk::lvgl_cpp::TextArea> _laps_area;
};

}  // namespace view
