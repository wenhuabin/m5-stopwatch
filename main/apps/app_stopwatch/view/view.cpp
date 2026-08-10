/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"

#include <cstdio>

using namespace view;
using namespace uitk::lvgl_cpp;

namespace {

constexpr int _panel_size = 466;

constexpr uint32_t _color_panel_bg     = 0x000000;
constexpr uint32_t _color_header_bg    = 0x17191B;
constexpr uint32_t _color_divider      = 0xB3D1D8;
constexpr uint32_t _color_time_bg      = 0x021A24;
constexpr uint32_t _color_title_bg     = 0x17191B;
constexpr uint32_t _color_stats_bg     = 0x17191B;
constexpr uint32_t _color_dot_active   = 0xBD0C0C;
constexpr uint32_t _color_dot_inactive = 0x282525;
constexpr uint32_t _color_badge_bg     = 0x373232;
constexpr uint32_t _color_badge_inner  = 0x7F0F0F;
constexpr uint32_t _color_laps_bg      = 0x99C0CD;
constexpr uint32_t _color_white        = 0xFFFFFF;
constexpr uint32_t _color_dim_1        = 0x5B6675;
constexpr uint32_t _color_dim_2        = 0x808080;
constexpr uint32_t _color_dim_3        = 0x575656;
constexpr uint32_t _color_dim_4        = 0x534C53;
constexpr uint32_t _color_dim_5        = 0x5A5757;
constexpr uint32_t _color_hint_blue    = 0x83A9D1;
constexpr uint32_t _color_value_blue   = 0x86AECC;
constexpr uint32_t _color_time2        = 0xA3A0A3;
constexpr uint32_t _color_m5stack      = 0x83ACB6;

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

    /* ---------------------------- Background panels --------------------------- */
    _header_panel = std::make_unique<Container>(_panel->get());
    _header_panel->align(LV_ALIGN_CENTER, 0, -211);
    _header_panel->setSize(143, 50);
    _header_panel->setRadius(0);
    _header_panel->setBorderWidth(0);
    _header_panel->setBgColor(lv_color_hex(_color_header_bg));
    _header_panel->setBgOpa(LV_OPA_COVER);

    _time_bg_panel = std::make_unique<Container>(_panel->get());
    _time_bg_panel->align(LV_ALIGN_CENTER, -83, 0);
    _time_bg_panel->setSize(398, 201);
    _time_bg_panel->setRadius(4);
    _time_bg_panel->setBorderWidth(0);
    _time_bg_panel->setBgColor(lv_color_hex(_color_time_bg));
    _time_bg_panel->setBgOpa(LV_OPA_COVER);

    _title_bg_panel = std::make_unique<Container>(_panel->get());
    _title_bg_panel->align(LV_ALIGN_CENTER, 42, -122);
    _title_bg_panel->setSize(148, 26);
    _title_bg_panel->setRadius(1);
    _title_bg_panel->setBorderWidth(0);
    _title_bg_panel->setBgColor(lv_color_hex(_color_title_bg));
    _title_bg_panel->setBgOpa(LV_OPA_COVER);

    _stats_bg_panel = std::make_unique<Container>(_panel->get());
    _stats_bg_panel->align(LV_ALIGN_CENTER, -102, 180);
    _stats_bg_panel->setSize(202, 112);
    _stats_bg_panel->setRadius(0);
    _stats_bg_panel->setBorderWidth(0);
    _stats_bg_panel->setBgColor(lv_color_hex(_color_stats_bg));
    _stats_bg_panel->setBgOpa(LV_OPA_COVER);

    _divider = std::make_unique<Container>(_panel->get());
    _divider->align(LV_ALIGN_CENTER, -14, 19);
    _divider->setSize(381, 2);
    _divider->setRadius(3);
    _divider->setBorderWidth(0);
    _divider->setBgColor(lv_color_hex(_color_divider));
    _divider->setBgOpa(LV_OPA_COVER);

    /* ---------------------------------- Header --------------------------------- */
    _m5stack_label = std::make_unique<Label>(_panel->get());
    _m5stack_label->setText("M5Stack");
    _m5stack_label->setTextColor(lv_color_hex(_color_m5stack));
    _m5stack_label->align(LV_ALIGN_CENTER, 7, -148);

    /* ----------------------------- Time / title area --------------------------- */
    _time_label = std::make_unique<Label>(_panel->get());
    _time_label->setText("00:00");
    _time_label->setTextFont(&lv_font_montserrat_36);
    _time_label->setTextColor(lv_color_hex(_color_white));
    _time_label->align(LV_ALIGN_CENTER, -81, -23);

    _centis_label = std::make_unique<Label>(_panel->get());
    _centis_label->setText("00");
    _centis_label->setTextFont(&lv_font_montserrat_18);
    _centis_label->setTextColor(lv_color_hex(_color_time2));
    _centis_label->align(LV_ALIGN_CENTER, 76, -41);

    _title_label = std::make_unique<Label>(_panel->get());
    _title_label->setText("STOPWATCH");
    _title_label->setTextColor(lv_color_hex(_color_white));
    _title_label->align(LV_ALIGN_CENTER, 21, -121);

    _minutes_caption = std::make_unique<Label>(_panel->get());
    _minutes_caption->setText("MINUTES");
    _minutes_caption->setTextColor(lv_color_hex(_color_dim_1));
    _minutes_caption->align(LV_ALIGN_CENTER, -157, -82);

    _seconds_caption = std::make_unique<Label>(_panel->get());
    _seconds_caption->setText("SECONDS");
    _seconds_caption->setTextColor(lv_color_hex(_color_dim_1));
    _seconds_caption->align(LV_ALIGN_CENTER, -34, -82);

    _centis_caption = std::make_unique<Label>(_panel->get());
    _centis_caption->setText("SEC/100");
    _centis_caption->setTextFont(&lv_font_montserrat_10);
    _centis_caption->setTextColor(lv_color_hex(_color_dim_2));
    _centis_caption->align(LV_ALIGN_CENTER, 74, -15);

    /* ------------------------------ State dots ---------------------------------*/
    _state_dot_start = std::make_unique<Container>(_panel->get());
    _state_dot_start->align(LV_ALIGN_CENTER, 132, -91);
    _state_dot_start->setSize(18, 18);
    _state_dot_start->setRadius(2);
    _state_dot_start->setBorderWidth(0);
    _state_dot_start->setBgOpa(LV_OPA_COVER);

    _state_dot_stop = std::make_unique<Container>(_panel->get());
    _state_dot_stop->align(LV_ALIGN_CENTER, 132, -68);
    _state_dot_stop->setSize(18, 18);
    _state_dot_stop->setRadius(2);
    _state_dot_stop->setBorderWidth(0);
    _state_dot_stop->setBgOpa(LV_OPA_COVER);

    _state_dot_reset = std::make_unique<Container>(_panel->get());
    _state_dot_reset->align(LV_ALIGN_CENTER, 132, -45);
    _state_dot_reset->setSize(18, 18);
    _state_dot_reset->setRadius(2);
    _state_dot_reset->setBorderWidth(0);
    _state_dot_reset->setBgOpa(LV_OPA_COVER);

    _lap_save_hint = std::make_unique<Label>(_panel->get());
    _lap_save_hint->setText("LAP SAVE");
    _lap_save_hint->setTextFont(&lv_font_montserrat_10);
    _lap_save_hint->setTextColor(lv_color_hex(_color_dim_3));
    _lap_save_hint->align(LV_ALIGN_CENTER, -123, -173);

    _start_stop_hint = std::make_unique<Label>(_panel->get());
    _start_stop_hint->setText("START\nSTOP");
    _start_stop_hint->setTextFont(&lv_font_montserrat_10);
    _start_stop_hint->setTextColor(lv_color_hex(_color_dim_3));
    _start_stop_hint->align(LV_ALIGN_CENTER, 142, -151);

    _reset_start_stop_hint = std::make_unique<Label>(_panel->get());
    _reset_start_stop_hint->setText("RESET\nSTART\nSTOP");
    _reset_start_stop_hint->setTextFont(&lv_font_montserrat_18);
    _reset_start_stop_hint->setTextColor(lv_color_hex(_color_hint_blue));
    _reset_start_stop_hint->align(LV_ALIGN_CENTER, 175, -69);

    /* ------------------------------- Chip badge --------------------------------*/
    _chip_badge_panel = std::make_unique<Container>(_panel->get());
    _chip_badge_panel->align(LV_ALIGN_CENTER, 156, -123);
    _chip_badge_panel->setSize(73, 27);
    _chip_badge_panel->setRadius(4);
    _chip_badge_panel->setBorderWidth(0);
    _chip_badge_panel->setBgColor(lv_color_hex(_color_badge_bg));
    _chip_badge_panel->setBgOpa(LV_OPA_COVER);

    _chip_badge_inner = std::make_unique<Container>(_chip_badge_panel->get());
    _chip_badge_inner->align(LV_ALIGN_CENTER, -13, -1);
    _chip_badge_inner->setSize(36, 21);
    _chip_badge_inner->setRadius(4);
    _chip_badge_inner->setBorderWidth(0);
    _chip_badge_inner->setBgColor(lv_color_hex(_color_badge_inner));
    _chip_badge_inner->setBgOpa(LV_OPA_COVER);

    _chip_badge_label_1 = std::make_unique<Label>(_chip_badge_panel->get());
    _chip_badge_label_1->setText("ESP");
    _chip_badge_label_1->align(LV_ALIGN_CENTER, -11, 1);

    _chip_badge_label_2 = std::make_unique<Label>(_chip_badge_panel->get());
    _chip_badge_label_2->setText("S3");
    _chip_badge_label_2->align(LV_ALIGN_CENTER, 20, 1);

    /* --------------------------------- Lap stats -------------------------------*/
    _current_lap_caption = std::make_unique<Label>(_panel->get());
    _current_lap_caption->setText("Current lap:");
    _current_lap_caption->setTextFont(&lv_font_montserrat_16);
    _current_lap_caption->setTextColor(lv_color_hex(_color_dim_4));
    _current_lap_caption->align(LV_ALIGN_CENTER, -153, 35);

    _current_lap_value = std::make_unique<Label>(_panel->get());
    _current_lap_value->setText("1");
    _current_lap_value->setTextFont(&lv_font_montserrat_16);
    _current_lap_value->setTextColor(lv_color_hex(_color_dim_4));
    _current_lap_value->align(LV_ALIGN_CENTER, -42, 35);

    _best_lap_caption = std::make_unique<Label>(_panel->get());
    _best_lap_caption->setText("BEST LAP:");
    _best_lap_caption->setTextFont(&lv_font_montserrat_16);
    _best_lap_caption->setTextColor(lv_color_hex(_color_dim_5));
    _best_lap_caption->align(LV_ALIGN_CENTER, -158, 58);

    _best_lap_value = std::make_unique<Label>(_panel->get());
    _best_lap_value->setText("--:--.--");
    _best_lap_value->setTextFont(&lv_font_montserrat_16);
    _best_lap_value->setTextColor(lv_color_hex(_color_value_blue));
    _best_lap_value->align(LV_ALIGN_CENTER, -41, 58);

    _worst_lap_caption = std::make_unique<Label>(_panel->get());
    _worst_lap_caption->setText("WORST LAP:");
    _worst_lap_caption->setTextFont(&lv_font_montserrat_16);
    _worst_lap_caption->setTextColor(lv_color_hex(_color_dim_5));
    _worst_lap_caption->align(LV_ALIGN_CENTER, -148, 78);

    _worst_lap_value = std::make_unique<Label>(_panel->get());
    _worst_lap_value->setText("--:--.--");
    _worst_lap_value->setTextFont(&lv_font_montserrat_16);
    _worst_lap_value->setTextColor(lv_color_hex(_color_value_blue));
    _worst_lap_value->align(LV_ALIGN_CENTER, -41, 78);

    _laps_caption = std::make_unique<Label>(_panel->get());
    _laps_caption->setText("LAPS:");
    _laps_caption->setTextColor(lv_color_hex(_color_dim_2));
    _laps_caption->align(LV_ALIGN_CENTER, 144, 6);

    _laps_area = std::make_unique<TextArea>(_panel->get());
    _laps_area->align(LV_ALIGN_CENTER, 93, 146);
    _laps_area->setSize(170, 238);
    _laps_area->setRadius(0);
    _laps_area->setBorderWidth(0);
    _laps_area->setBgColor(lv_color_hex(_color_laps_bg));
    _laps_area->setBgOpa(LV_OPA_COVER);
    _laps_area->setTextColor(lv_color_hex(0x000000));
    _laps_area->setTextFont(&lv_font_montserrat_18);
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
    const lv_color_t active_color   = lv_color_hex(_color_dot_active);
    const lv_color_t inactive_color = lv_color_hex(_color_dot_inactive);

    // The dot lit up is the action the *next* button press will perform,
    // matching the reference project: idle -> "start" lit, running ->
    // "stop" lit, paused -> "reset" lit.
    _state_dot_start->setBgColor(phase == StopwatchPhase::Idle ? active_color : inactive_color);
    _state_dot_stop->setBgColor(phase == StopwatchPhase::Running ? active_color : inactive_color);
    _state_dot_reset->setBgColor(phase == StopwatchPhase::Paused ? active_color : inactive_color);
}

void StopwatchView::addLap(std::size_t lapNumber, uint32_t lapMs, bool isBest, bool isWorst)
{
    char formatted[16] = {};
    formatLap(lapMs, formatted, sizeof(formatted));

    char line[48] = {};
    snprintf(line, sizeof(line), "%02u   %s\n", static_cast<unsigned>(lapNumber), formatted);
    _laps_area->addText(line);

    char current[8] = {};
    snprintf(current, sizeof(current), "%u", static_cast<unsigned>(lapNumber));
    _current_lap_value->setText(current);

    if (isBest) {
        _best_lap_value->setText(formatted);
    }
    if (isWorst) {
        _worst_lap_value->setText(formatted);
    }
}

void StopwatchView::resetLaps()
{
    _laps_area->setText("");
    _current_lap_value->setText("1");
    _best_lap_value->setText("--:--.--");
    _worst_lap_value->setText("--:--.--");
}
