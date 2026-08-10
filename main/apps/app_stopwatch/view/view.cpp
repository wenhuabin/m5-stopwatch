/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"

#include <assets/assets.h>
#include <cstdio>

using namespace view;
using namespace uitk::lvgl_cpp;

namespace {

constexpr int _panel_size = 466;

constexpr uint32_t _color_panel_bg   = 0x000000;
constexpr uint32_t _color_time_bg    = 0x021A24;
constexpr uint32_t _color_white      = 0xFFFFFF;
constexpr uint32_t _color_time2      = 0xA3A0A3;
constexpr uint32_t _color_state_dim  = 0x6B6969;
constexpr uint32_t _color_state_lit  = 0xE33A3A;
constexpr uint32_t _color_lap_value  = 0x86AECC;
constexpr uint32_t _color_lap_dim    = 0xB8B8B8;
constexpr uint32_t _color_laps_bg    = 0x14171C;
constexpr uint32_t _color_laps_text  = 0xE8ECEF;
constexpr uint32_t _color_batt_track = 0x2B2B2B;
constexpr uint32_t _color_batt_fill  = 0x99C0CD;

void formatClock(uint32_t elapsedMs, char* clockOut, std::size_t clockLen, char* centisOut, std::size_t centisLen)
{
    const uint32_t total_centis = elapsedMs / 10;
    const uint32_t minutes      = (total_centis / 6000) % 100;
    const uint32_t seconds      = (total_centis / 100) % 60;
    const uint32_t centis       = total_centis % 100;
    snprintf(clockOut, clockLen, "%02u:%02u", static_cast<unsigned>(minutes), static_cast<unsigned>(seconds));
    snprintf(centisOut, centisLen, "%02u", static_cast<unsigned>(centis));
}

void formatLap(uint32_t lapMs, char* out, std::size_t outLen)
{
    const uint32_t total_centis = lapMs / 10;
    const uint32_t minutes      = (total_centis / 6000) % 100;
    const uint32_t seconds      = (total_centis / 100) % 60;
    const uint32_t centis       = total_centis % 100;
    snprintf(out, outLen, "%02u:%02u.%02u", static_cast<unsigned>(minutes), static_cast<unsigned>(seconds),
              static_cast<unsigned>(centis));
}

}  // namespace

void StopwatchView::init(lv_obj_t* parent)
{
    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_panel_size, _panel_size);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->setBgColor(lv_color_hex(_color_panel_bg));
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    /* ----------------------------- Battery + title ------------------------------*/
    _battery_bar = std::make_unique<Bar>(_panel->get());
    _battery_bar->setSize(30, 12);
    _battery_bar->setRadius(4);
    _battery_bar->setRadius(0, LV_PART_INDICATOR);
    _battery_bar->setBgColor(lv_color_hex(_color_batt_track));
    _battery_bar->setBgColor(lv_color_hex(_color_batt_fill), LV_PART_INDICATOR);
    _battery_bar->setBgOpa(LV_OPA_COVER);
    _battery_bar->setRange(0, 100);
    _battery_bar->setValue(0);
    _battery_bar->setPadding(1, 1, 1, 1);
    _battery_bar->setOutlineWidth(1);
    _battery_bar->setOutlineColor(lv_color_hex(_color_batt_fill));
    _battery_bar->setRadius(3, LV_PART_INDICATOR);
    _battery_bar->align(LV_ALIGN_CENTER, -20, -205);

    _battery_nub = std::make_unique<Container>(_panel->get());
    _battery_nub->setBgColor(lv_color_hex(_color_panel_bg));
    _battery_nub->setScrollbarMode(LV_SCROLLBAR_MODE_OFF);
    _battery_nub->setBorderWidth(0);
    _battery_nub->setSize(4, 4);
    _battery_nub->setRadius(4);
    _battery_nub->setOutlineWidth(1);
    _battery_nub->setOutlineColor(lv_color_hex(_color_batt_fill));
    lv_obj_align_to(_battery_nub->get(), _battery_bar->get(), LV_ALIGN_CENTER, 16, 0);

    _battery_charge_icon = std::make_unique<Image>(_panel->get());
    _battery_charge_icon->setSrc(&icon_bat_lightning);
    _battery_charge_icon->setImageRecolor(lv_color_hex(_color_batt_fill));
    _battery_charge_icon->setImageRecolorOpa(LV_OPA_COVER);
    _battery_charge_icon->setHidden(true);
    lv_obj_align_to(_battery_charge_icon->get(), _battery_bar->get(), LV_ALIGN_CENTER, 0, 0);

    _battery_label = std::make_unique<Label>(_panel->get());
    _battery_label->setText("--%");
    _battery_label->setTextFont(&lv_font_montserrat_16);
    _battery_label->setTextColor(lv_color_hex(_color_white));
    _battery_label->align(LV_ALIGN_CENTER, 24, -205);

    _title_label = std::make_unique<Label>(_panel->get());
    _title_label->setText("STOPWATCH");
    _title_label->setTextFont(&lv_font_montserrat_24);
    _title_label->setTextColor(lv_color_hex(_color_white));
    _title_label->align(LV_ALIGN_CENTER, 0, -155);

    /* --------------------------------- State row --------------------------------*/
    constexpr int _state_dot_size = 10;
    constexpr int _state_row_y    = -115;

    _state_dot_reset = std::make_unique<Container>(_panel->get());
    _state_dot_reset->setSize(_state_dot_size, _state_dot_size);
    _state_dot_reset->setRadius(2);
    _state_dot_reset->setBorderWidth(0);
    _state_dot_reset->setBgOpa(LV_OPA_COVER);
    _state_dot_reset->align(LV_ALIGN_CENTER, -142, _state_row_y);

    _state_label_reset = std::make_unique<Label>(_panel->get());
    _state_label_reset->setText("RESET");
    _state_label_reset->setTextFont(&lv_font_montserrat_16);
    _state_label_reset->align(LV_ALIGN_CENTER, -104, _state_row_y);

    _state_dot_start = std::make_unique<Container>(_panel->get());
    _state_dot_start->setSize(_state_dot_size, _state_dot_size);
    _state_dot_start->setRadius(2);
    _state_dot_start->setBorderWidth(0);
    _state_dot_start->setBgOpa(LV_OPA_COVER);
    _state_dot_start->align(LV_ALIGN_CENTER, -32, _state_row_y);

    _state_label_start = std::make_unique<Label>(_panel->get());
    _state_label_start->setText("START");
    _state_label_start->setTextFont(&lv_font_montserrat_16);
    _state_label_start->align(LV_ALIGN_CENTER, 6, _state_row_y);

    _state_dot_stop = std::make_unique<Container>(_panel->get());
    _state_dot_stop->setSize(_state_dot_size, _state_dot_size);
    _state_dot_stop->setRadius(2);
    _state_dot_stop->setBorderWidth(0);
    _state_dot_stop->setBgOpa(LV_OPA_COVER);
    _state_dot_stop->align(LV_ALIGN_CENTER, 82, _state_row_y);

    _state_label_stop = std::make_unique<Label>(_panel->get());
    _state_label_stop->setText("STOP");
    _state_label_stop->setTextFont(&lv_font_montserrat_16);
    _state_label_stop->align(LV_ALIGN_CENTER, 116, _state_row_y);

    /* ----------------------------- Time area (full width) -----------------------*/
    _time_bg_panel = std::make_unique<Container>(_panel->get());
    _time_bg_panel->align(LV_ALIGN_CENTER, 0, -30);
    _time_bg_panel->setSize(420, 110);
    _time_bg_panel->setRadius(8);
    _time_bg_panel->setBorderWidth(0);
    _time_bg_panel->setBgColor(lv_color_hex(_color_time_bg));
    _time_bg_panel->setBgOpa(LV_OPA_COVER);
    _time_bg_panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _time_label = std::make_unique<Label>(_time_bg_panel->get());
    _time_label->setText("00:00");
    _time_label->setTextFont(&lv_font_montserrat_48);
    _time_label->setTextColor(lv_color_hex(_color_white));
    _time_label->align(LV_ALIGN_CENTER, 0, -6);
    // No bigger built-in font is available (48px is LVGL's largest
    // Montserrat size), so scale the rendered glyphs up 1.5x instead.
    // The transform pivot defaults to the widget's top-left corner (0,0),
    // not its center, so without explicitly centering the pivot the scale
    // grows the glyphs rightward/downward instead of around their middle.
    _time_label->setTransformPivot(LV_PCT(50), LV_PCT(50));
    lv_obj_set_style_transform_scale_x(_time_label->get(), 384, 0);
    lv_obj_set_style_transform_scale_y(_time_label->get(), 384, 0);

    _centis_label = std::make_unique<Label>(_time_bg_panel->get());
    _centis_label->setText("00");
    _centis_label->setTextFont(&lv_font_montserrat_18);
    _centis_label->setTextColor(lv_color_hex(_color_time2));
    _centis_label->align(LV_ALIGN_CENTER, 140, 22);

    /* ------------------------------- Lap stats row -------------------------------*/
    _current_lap_label = std::make_unique<Label>(_panel->get());
    _current_lap_label->setText("LAP --");
    _current_lap_label->setTextFont(&lv_font_montserrat_16);
    _current_lap_label->setTextColor(lv_color_hex(_color_lap_dim));
    _current_lap_label->align(LV_ALIGN_CENTER, -155, 55);

    _best_lap_label = std::make_unique<Label>(_panel->get());
    _best_lap_label->setText("BEST --:--.--");
    _best_lap_label->setTextFont(&lv_font_montserrat_16);
    _best_lap_label->setTextColor(lv_color_hex(_color_lap_value));
    _best_lap_label->align(LV_ALIGN_CENTER, -15, 55);

    _worst_lap_label = std::make_unique<Label>(_panel->get());
    _worst_lap_label->setText("WORST --:--.--");
    _worst_lap_label->setTextFont(&lv_font_montserrat_16);
    _worst_lap_label->setTextColor(lv_color_hex(_color_lap_value));
    _worst_lap_label->align(LV_ALIGN_CENTER, 150, 55);

    /* ---------------------------------- Lap list ---------------------------------*/
    _laps_area = std::make_unique<TextArea>(_panel->get());
    _laps_area->align(LV_ALIGN_CENTER, 0, 159);
    _laps_area->setSize(240, 148);
    _laps_area->setRadius(6);
    _laps_area->setBorderWidth(0);
    _laps_area->setBgColor(lv_color_hex(_color_laps_bg));
    _laps_area->setBgOpa(LV_OPA_COVER);
    _laps_area->setTextColor(lv_color_hex(_color_laps_text));
    _laps_area->setTextFont(&lv_font_montserrat_18);
    _laps_area->setTextAreaAlign(LV_TEXT_ALIGN_CENTER);
    _laps_area->setCursorClickPos(false);
    _laps_area->removeFlag(LV_OBJ_FLAG_CLICK_FOCUSABLE);

    setPhase(StopwatchPhase::Idle);
}

void StopwatchView::setElapsed(uint32_t elapsedMs)
{
    char clock[8]  = {};
    char centis[4] = {};
    formatClock(elapsedMs, clock, sizeof(clock), centis, sizeof(centis));
    _time_label->setText(clock);
    _centis_label->setText(centis);
}

void StopwatchView::setPhase(StopwatchPhase phase)
{
    const lv_color_t lit_color = lv_color_hex(_color_state_lit);
    const lv_color_t dim_color = lv_color_hex(_color_state_dim);

    // The word lit up is the action the *next* button press will perform:
    // idle -> "start" lit, running -> "stop" lit, paused -> "reset" lit.
    const bool reset_active = phase == StopwatchPhase::Paused;
    const bool start_active = phase == StopwatchPhase::Idle;
    const bool stop_active  = phase == StopwatchPhase::Running;

    _state_label_reset->setTextColor(reset_active ? lit_color : dim_color);
    _state_label_start->setTextColor(start_active ? lit_color : dim_color);
    _state_label_stop->setTextColor(stop_active ? lit_color : dim_color);

    _state_dot_reset->setBgColor(reset_active ? lit_color : dim_color);
    _state_dot_start->setBgColor(start_active ? lit_color : dim_color);
    _state_dot_stop->setBgColor(stop_active ? lit_color : dim_color);
}

void StopwatchView::addLap(std::size_t lapNumber, uint32_t lapMs, bool isBest, bool isWorst)
{
    char formatted[16] = {};
    formatLap(lapMs, formatted, sizeof(formatted));

    char line[48] = {};
    snprintf(line, sizeof(line), "%02u   %s\n", static_cast<unsigned>(lapNumber), formatted);
    _laps_area->addText(line);

    char current[16] = {};
    snprintf(current, sizeof(current), "LAP %u", static_cast<unsigned>(lapNumber));
    _current_lap_label->setText(current);

    if (isBest) {
        char best[24] = {};
        snprintf(best, sizeof(best), "BEST %s", formatted);
        _best_lap_label->setText(best);
    }
    if (isWorst) {
        char worst[24] = {};
        snprintf(worst, sizeof(worst), "WORST %s", formatted);
        _worst_lap_label->setText(worst);
    }
}

void StopwatchView::resetLaps()
{
    _laps_area->setText("");
    _current_lap_label->setText("LAP --");
    _best_lap_label->setText("BEST --:--.--");
    _worst_lap_label->setText("WORST --:--.--");
}

void StopwatchView::setBatteryLevel(uint8_t percent, bool charging)
{
    _battery_bar->setValue(percent);
    _battery_charge_icon->setHidden(!charging);

    char text[8] = {};
    snprintf(text, sizeof(text), "%u%%", static_cast<unsigned>(percent));
    _battery_label->setText(text);
}
