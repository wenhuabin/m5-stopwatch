/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"

#include <assets/assets.h>

using namespace view;
using namespace uitk::lvgl_cpp;

namespace {

constexpr int _panel_size = 466;

constexpr uint32_t _color_panel_bg = 0xFFFFFF;
constexpr uint32_t _color_title    = 0x1A1A1A;

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

    _title_label = std::make_unique<Label>(_panel->get());
    _title_label->setText("Pendulum");
    _title_label->setTextFont(&TodoTitleBold40);
    _title_label->setTextColor(lv_color_hex(_color_title));
    _title_label->align(LV_ALIGN_CENTER, 0, -195);
}

bool PendulumView::consumeReleaseRequested()
{
    const bool requested = _release_requested;
    _release_requested    = false;
    return requested;
}
