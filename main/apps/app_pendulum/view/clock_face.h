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
    std::unique_ptr<uitk::lvgl_cpp::Container> _bezel;
    std::unique_ptr<uitk::lvgl_cpp::Container> _face;
    std::unique_ptr<uitk::lvgl_cpp::Container> _pivot_dot;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Label>> _numeral_labels;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Container>> _tick_marks;
    std::unique_ptr<uitk::lvgl_cpp::Container> _hour_hand;
    std::unique_ptr<uitk::lvgl_cpp::Container> _minute_hand;
    std::unique_ptr<uitk::lvgl_cpp::Container> _second_hand;
};

}  // namespace view
