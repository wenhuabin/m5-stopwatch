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
constexpr double _center_y   = 120.0;
constexpr int _face_radius   = 90;
constexpr int _bezel_thickness    = 8;
constexpr int _numeral_radius     = 72;
constexpr int _tick_inner_radius  = 78;
constexpr int _tick_outer_radius  = 88;
constexpr int _pivot_dot_size     = 8;
constexpr int _tick_dot_size      = 4;

constexpr uint32_t _color_face_bg    = 0xFFFFFF;
constexpr uint32_t _color_border     = 0x1A1A1A;
// Brass case bezel and pivot, matching the pendulum bob's palette for a
// bit of tactile depth/cohesion rather than a flat white disc.
constexpr uint32_t _color_bezel      = 0x8B6914;
constexpr uint32_t _color_pivot      = 0xB8860B;
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
    // Brass case bezel, drawn behind (and slightly larger than) the white
    // face so it shows as a raised metal ring; a soft shadow gives it a
    // bit of depth against the panel.
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
