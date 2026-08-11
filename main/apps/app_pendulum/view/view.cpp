/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"

#include <cmath>

using namespace view;
using namespace uitk::lvgl_cpp;

namespace {

constexpr int _panel_size = 466;

constexpr double _pivot_x     = 233.0;
constexpr double _pivot_y     = 100.0;
constexpr double _rod_length  = 180.0;
constexpr int _bob_radius     = 24;
constexpr int _pivot_dot_size = 12;
constexpr int _rod_width      = 4;

constexpr uint32_t _color_panel_bg = 0xFFFFFF;
constexpr uint32_t _color_pivot    = 0x1A1A1A;
constexpr uint32_t _color_rod      = 0x4A4A4A;
constexpr uint32_t _color_bob      = 0x5865F2;

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

    _pivot_dot = std::make_unique<Container>(_panel->get());
    _pivot_dot->setSize(_pivot_dot_size, _pivot_dot_size);
    _pivot_dot->setRadius(LV_RADIUS_CIRCLE);
    _pivot_dot->setBorderWidth(0);
    _pivot_dot->setBgColor(lv_color_hex(_color_pivot));
    _pivot_dot->setBgOpa(LV_OPA_COVER);
    _pivot_dot->setPos(static_cast<int32_t>(_pivot_x) - _pivot_dot_size / 2,
                        static_cast<int32_t>(_pivot_y) - _pivot_dot_size / 2);

    _rod = std::make_unique<Line>(_panel->get());
    _rod->setPos(0, 0);
    _rod->setSize(_panel_size, _panel_size);
    _rod->setBgOpa(LV_OPA_TRANSP);
    _rod->setBorderWidth(0);
    _rod->setLineColor(lv_color_hex(_color_rod));
    _rod->setLineWidth(_rod_width);
    _rod->setLineRounded(true);

    _bob = std::make_unique<Container>(_panel->get());
    _bob->setSize(_bob_radius * 2, _bob_radius * 2);
    _bob->setRadius(LV_RADIUS_CIRCLE);
    _bob->setBorderWidth(0);
    _bob->setBgColor(lv_color_hex(_color_bob));
    _bob->setBgOpa(LV_OPA_COVER);
}

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

bool PendulumView::consumeReleaseRequested()
{
    const bool requested = _release_requested;
    _release_requested    = false;
    return requested;
}
