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
constexpr double _center_y   = 165.0;
// The panel is a 466x466 square Container, so its own center (used by
// LV_ALIGN_CENTER offsets below) is fixed at half its size regardless of
// where the dial itself is centered.
constexpr double _panel_center_y  = 233.0;
constexpr int _face_radius   = 86;
constexpr int _bezel_thickness    = 14;
constexpr int _numeral_radius     = 68;
constexpr int _tick_inner_radius  = 74;
constexpr int _tick_outer_radius  = 84;
constexpr int _pivot_dot_size     = 8;
constexpr int _tick_width         = 3;

constexpr uint32_t _color_face_bg    = 0xF1E4C2;  // cream dial
constexpr uint32_t _color_border     = 0x1F2E3D;  // dark navy dial rim
// Lighter blue-gray face surround, matching the case body's family rather
// than the earlier brass medallion look.
constexpr uint32_t _color_bezel      = 0x7189A0;
constexpr uint32_t _color_pivot      = 0xB5342B;  // red center dot
constexpr uint32_t _color_numeral    = 0x1F2E3D;
constexpr uint32_t _color_tick       = 0x1F2E3D;
constexpr uint32_t _color_hour_hand   = 0xB5342B;
constexpr uint32_t _color_minute_hand = 0xB5342B;
constexpr uint32_t _color_second_hand = 0xB5342B;

constexpr int _hour_hand_length   = 33;
constexpr int _hour_hand_width    = 5;
constexpr int _minute_hand_length = 53;
constexpr int _minute_hand_width  = 4;
constexpr int _second_hand_length = 62;
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
    // Lighter blue-gray face surround, drawn behind (and slightly larger
    // than) the cream face so it shows as a raised ring; a soft shadow
    // gives it a bit of depth against the case.
    const int bezel_radius = _face_radius + _bezel_thickness;
    _bezel = std::make_unique<Container>(parent);
    _bezel->setSize(bezel_radius * 2, bezel_radius * 2);
    _bezel->setPos(static_cast<int32_t>(_center_x) - bezel_radius, static_cast<int32_t>(_center_y) - bezel_radius);
    _bezel->setRadius(LV_RADIUS_CIRCLE);
    _bezel->setBorderWidth(0);
    _bezel->setBgColor(lv_color_hex(_color_bezel));
    _bezel->setBgOpa(LV_OPA_COVER);
    _bezel->setShadowWidth(14);
    _bezel->setShadowOffsetY(5);
    _bezel->setShadowColor(lv_color_hex(0x000000));
    _bezel->setShadowOpa(70);
    _bezel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _bezel->removeFlag(LV_OBJ_FLAG_CLICKABLE);

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

        // A thin rectangle, positioned so its own center sits at the tick's
        // midpoint radius and then rotated around that same center: since
        // the position is already along the angle from the dial's center,
        // rotating the (initially vertical) rectangle by that same angle
        // aligns it with the radial direction -- no off-center pivot needed.
        const double tick_len   = _tick_outer_radius - _tick_inner_radius;
        const double mid_radius = (_tick_inner_radius + _tick_outer_radius) / 2.0;
        auto tick = std::make_unique<Container>(parent);
        tick->setSize(_tick_width, static_cast<int32_t>(tick_len));
        tick->setRadius(_tick_width / 2);
        tick->setBorderWidth(0);
        tick->setBgColor(lv_color_hex(_color_tick));
        tick->setBgOpa(LV_OPA_COVER);
        const double tick_cx = _center_x + mid_radius * std::sin(angle);
        const double tick_cy = _center_y - mid_radius * std::cos(angle);
        tick->setPos(static_cast<int32_t>(tick_cx) - _tick_width / 2,
                     static_cast<int32_t>(tick_cy) - static_cast<int32_t>(tick_len) / 2);
        tick->setTransformPivot(LV_PCT(50), LV_PCT(50));
        tick->setRotation(static_cast<int32_t>(angle * 180.0 / kPi * 10.0));
        _tick_marks.push_back(std::move(tick));

        const int numeral = (h == 0) ? 12 : h;
        auto label = std::make_unique<Label>(parent);
        label->setText(std::to_string(numeral));
        label->setTextFont(&lv_font_montserrat_14);
        label->setTextColor(lv_color_hex(_color_numeral));
        const double num_x = _center_x + _numeral_radius * std::sin(angle);
        const double num_y = _center_y - _numeral_radius * std::cos(angle);
        label->align(LV_ALIGN_CENTER, static_cast<int32_t>(num_x - _center_x),
                     static_cast<int32_t>(num_y - _panel_center_y));
        _numeral_labels.push_back(std::move(label));
    }

    _pivot_dot = std::make_unique<Container>(parent);
    _pivot_dot->setSize(_pivot_dot_size, _pivot_dot_size);
    _pivot_dot->setRadius(LV_RADIUS_CIRCLE);
    _pivot_dot->setBorderWidth(0);
    _pivot_dot->setBgColor(lv_color_hex(_color_pivot));
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
