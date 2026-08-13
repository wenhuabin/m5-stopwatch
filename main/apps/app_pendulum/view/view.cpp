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

// Wall-clock case (navy rounded rect + a narrower "cap" band inset at its
// top), sized/positioned so every rounded corner clears the round
// 233px-radius bezel with margin: worst-case corner-arc-center distance
// from panel center (233,233) plus the corner radius is ~227.0px, 6px under
// the 233px bezel radius.
constexpr double _case_cx      = 233.0;
constexpr double _case_cy      = 230.0;
constexpr int _case_w          = 260;
constexpr int _case_h          = 390;
constexpr int _case_radius     = 26;
constexpr int _case_cap_h      = 22;
constexpr int _case_cap_inset  = 10;  // narrower than the case on each side
constexpr int _case_cap_radius = 8;

constexpr double _pivot_x     = 233.0;
constexpr double _pivot_y     = 283.0;
constexpr double _rod_length  = 93.0;
constexpr int _bob_radius     = 22;
constexpr int _pivot_dot_size = 10;
constexpr int _rod_width      = 3;

constexpr uint32_t _color_panel_bg = 0xEDEDED;  // backdrop outside the case
constexpr uint32_t _color_case_body = 0x3F5872;
constexpr uint32_t _color_case_cap  = 0x33475C;
constexpr uint32_t _color_pivot     = 0x1A1A1A;
constexpr uint32_t _color_rod       = 0xD8D8D8;
constexpr uint32_t _color_rod_edge  = 0xBFBFBF;
// Flat tan/gold bob: a single fill with a slightly darker edge and a soft
// drop shadow for grounding, rather than the earlier rim/fill/shine stack.
constexpr uint32_t _color_bob_fill = 0xD9AF6C;
constexpr uint32_t _color_bob_edge = 0xB98E4E;

// Kept under 80 deg (was the value used at the shorter rod length) so the
// longer rod's bob still stays inside the round display at full swing.
constexpr double _max_drag_angle_rad = 70.0 * 3.14159265358979323846 / 180.0;

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
    _panel->onPressed(handlePressed, this);
    _panel->addEventCb(handlePressing, LV_EVENT_PRESSING, this);
    _panel->onRelease(handleReleased, this);

    _case_body = std::make_unique<Container>(_panel->get());
    _case_body->setSize(_case_w, _case_h);
    _case_body->setPos(static_cast<int32_t>(_case_cx - _case_w / 2.0), static_cast<int32_t>(_case_cy - _case_h / 2.0));
    _case_body->setRadius(_case_radius);
    _case_body->setBorderWidth(0);
    _case_body->setBgColor(lv_color_hex(_color_case_body));
    _case_body->setBgOpa(LV_OPA_COVER);
    _case_body->setShadowWidth(16);
    _case_body->setShadowOffsetY(6);
    _case_body->setShadowColor(lv_color_hex(0x000000));
    _case_body->setShadowOpa(60);
    _case_body->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _case_body->removeFlag(LV_OBJ_FLAG_CLICKABLE);

    // A narrower, slightly darker band inset at the case's own top, standing
    // in for the reference image's overhanging cap/pediment: an overhanging
    // piece wide enough to read as a cap would have corners that fall
    // outside the round bezel this close to its top edge, so this band
    // stays inside the already-verified case footprint instead.
    const int case_top = static_cast<int32_t>(_case_cy - _case_h / 2.0);
    _case_cap = std::make_unique<Container>(_panel->get());
    _case_cap->setSize(_case_w - _case_cap_inset * 2, _case_cap_h);
    _case_cap->setPos(static_cast<int32_t>(_case_cx - (_case_w - _case_cap_inset * 2) / 2.0), case_top);
    _case_cap->setRadius(_case_cap_radius);
    _case_cap->setBorderWidth(0);
    _case_cap->setBgColor(lv_color_hex(_color_case_cap));
    _case_cap->setBgOpa(LV_OPA_COVER);
    _case_cap->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _case_cap->removeFlag(LV_OBJ_FLAG_CLICKABLE);

    _clock_face = std::make_unique<ClockFace>();
    _clock_face->init(_panel->get());

    _pivot_dot = std::make_unique<Container>(_panel->get());
    _pivot_dot->setSize(_pivot_dot_size, _pivot_dot_size);
    _pivot_dot->setRadius(LV_RADIUS_CIRCLE);
    _pivot_dot->setBorderWidth(0);
    _pivot_dot->setBgColor(lv_color_hex(_color_pivot));
    _pivot_dot->setBgOpa(LV_OPA_COVER);
    _pivot_dot->setPos(static_cast<int32_t>(_pivot_x) - _pivot_dot_size / 2,
                        static_cast<int32_t>(_pivot_y) - _pivot_dot_size / 2);

    // Drawn as a thin rectangle rotated around its top-center (the pivot)
    // rather than an lv_line: the line widget's own bounding box has to
    // span the whole swing arc, which made its per-frame invalidation
    // and self-sizing behave unreliably (missized bounds, a dangling
    // points pointer, and visible flicker while swinging). A rotated
    // rectangle only ever needs to invalidate its own small area.
    _rod = std::make_unique<Container>(_panel->get());
    _rod->setSize(_rod_width, static_cast<int32_t>(_rod_length));
    _rod->setPos(static_cast<int32_t>(_pivot_x) - _rod_width / 2, static_cast<int32_t>(_pivot_y));
    _rod->setRadius(_rod_width / 2);
    _rod->setBorderWidth(1);
    _rod->setBorderColor(lv_color_hex(_color_rod_edge));
    _rod->setBgColor(lv_color_hex(_color_rod));
    _rod->setBgOpa(LV_OPA_COVER);
    _rod->setTransformPivot(LV_PCT(50), 0);

    // Flat tan/gold bob: single fill, a slightly darker edge, and a soft
    // drop shadow for grounding.
    _bob = std::make_unique<Container>(_panel->get());
    _bob->setSize(_bob_radius * 2, _bob_radius * 2);
    _bob->setRadius(LV_RADIUS_CIRCLE);
    _bob->setBorderWidth(2);
    _bob->setBorderColor(lv_color_hex(_color_bob_edge));
    _bob->setBgColor(lv_color_hex(_color_bob_fill));
    _bob->setBgOpa(LV_OPA_COVER);
    _bob->setShadowWidth(10);
    _bob->setShadowOffsetY(4);
    _bob->setShadowColor(lv_color_hex(0x000000));
    _bob->setShadowOpa(70);
}

void PendulumView::setAngle(double thetaRad)
{
    const double bob_x = _pivot_x + _rod_length * std::sin(thetaRad);
    const double bob_y = _pivot_y + _rod_length * std::cos(thetaRad);

    // LVGL's positive transform_rotation is the opposite winding direction
    // from this view's theta (positive theta = swung toward +x), hence
    // the negation here.
    constexpr double kRadToDeciDeg = 180.0 / 3.14159265358979323846 * 10.0;
    _rod->setRotation(static_cast<int32_t>(-thetaRad * kRadToDeciDeg));

    const int32_t bob_xi = static_cast<int32_t>(bob_x);
    const int32_t bob_yi = static_cast<int32_t>(bob_y);
    _bob->setPos(bob_xi - _bob_radius, bob_yi - _bob_radius);
}

void PendulumView::setTime(uint8_t hour, uint8_t minute, uint8_t second)
{
    if (_clock_face) {
        _clock_face->setTime(hour, minute, second);
    }
}

bool PendulumView::consumeReleaseRequested()
{
    const bool requested = _release_requested;
    _release_requested    = false;
    return requested;
}

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
