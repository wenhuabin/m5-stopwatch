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
