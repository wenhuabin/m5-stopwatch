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

constexpr int _dialog_width         = 404;
constexpr int _dialog_height        = 202;
constexpr int _dialog_button_width  = 148;
constexpr int _dialog_button_height = 63;

constexpr uint32_t _dialog_bg_color      = 0x2B2B2B;
constexpr uint32_t _dialog_border_color  = 0x6A6A6A;
constexpr uint32_t _dialog_confirm_color = 0x5865F2;
constexpr uint32_t _dialog_cancel_color  = 0x515151;
constexpr uint32_t _dialog_label_color   = 0xFFFFFF;

constexpr uint32_t _color_panel_bg    = 0xFFFFFF;
constexpr uint32_t _color_title       = 0x1A1A1A;
constexpr uint32_t _color_hint        = 0x9E9E9E;
constexpr uint32_t _color_row_text    = 0x1A1A1A;
constexpr uint32_t _color_row_done    = 0xABABAB;
constexpr uint32_t _color_box_border  = 0xB0B0B0;
constexpr uint32_t _color_box_done    = 0x34A853;

constexpr int _row_height     = 44;
constexpr int _box_size       = 22;
constexpr int _box_x          = 6;
constexpr int _label_x        = 40;

}  // namespace

void TodoManageDialog::init(lv_obj_t* parent)
{
    _is_confirmed = false;
    _is_cancelled = false;

    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_dialog_width, _dialog_height);
    _panel->setBgColor(lv_color_hex(_dialog_bg_color));
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->setBorderColor(lv_color_hex(_dialog_border_color));
    _panel->setBorderWidth(3);
    _panel->setRadius(58);
    _panel->setPaddingAll(0);
    _panel->moveForeground();

    _label = std::make_unique<Label>(_panel->get());
    _label->setText("Manage todos\non phone?");
    _label->setTextFont(&MontserratSemiBold26);
    _label->setTextColor(lv_color_hex(_dialog_label_color));
    _label->align(LV_ALIGN_TOP_LEFT, 49, 30);

    _confirm_button = std::make_unique<Button>(_panel->get());
    _confirm_button->setSize(_dialog_button_width, _dialog_button_height);
    _confirm_button->align(LV_ALIGN_CENTER, -92, 46);
    _confirm_button->setRadius(LV_RADIUS_CIRCLE);
    _confirm_button->setBorderWidth(0);
    _confirm_button->setShadowWidth(0);
    _confirm_button->setBgColor(lv_color_hex(_dialog_confirm_color));
    _confirm_button->label().setText("Manage");
    _confirm_button->label().setTextFont(&lv_font_montserrat_24);
    _confirm_button->label().setTextColor(lv_color_hex(_dialog_label_color));
    _confirm_button->label().align(LV_ALIGN_CENTER, 0, 0);
    _confirm_button->onClick().connect([this]() { _is_confirmed = true; });

    _cancel_button = std::make_unique<Button>(_panel->get());
    _cancel_button->setSize(_dialog_button_width, _dialog_button_height);
    _cancel_button->align(LV_ALIGN_CENTER, 92, 46);
    _cancel_button->setRadius(LV_RADIUS_CIRCLE);
    _cancel_button->setBorderWidth(0);
    _cancel_button->setShadowWidth(0);
    _cancel_button->setBgColor(lv_color_hex(_dialog_cancel_color));
    _cancel_button->label().setText("Cancel");
    _cancel_button->label().setTextFont(&lv_font_montserrat_24);
    _cancel_button->label().setTextColor(lv_color_hex(_dialog_label_color));
    _cancel_button->label().align(LV_ALIGN_CENTER, 0, 0);
    _cancel_button->onClick().connect([this]() { _is_cancelled = true; });
}

void TodoView::init(lv_obj_t* parent)
{
    _pending_toggle_id = 0;
    _manage_requested  = false;

    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_panel_size, _panel_size);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->setBgColor(lv_color_hex(_color_panel_bg));
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _panel->addEventCb(handleLongPressed, LV_EVENT_LONG_PRESSED, this);

    _title_label = std::make_unique<Label>(_panel->get());
    _title_label->setText("Todo");
    _title_label->setTextFont(&lv_font_montserrat_28);
    _title_label->setTextColor(lv_color_hex(_color_title));
    _title_label->align(LV_ALIGN_CENTER, 0, -195);

    _list_panel = std::make_unique<Container>(_panel->get());
    _list_panel->align(LV_ALIGN_CENTER, 0, 20);
    _list_panel->setSize(410, 340);
    _list_panel->setRadius(0);
    _list_panel->setBorderWidth(0);
    _list_panel->setPaddingAll(0);
    _list_panel->setBgOpa(LV_OPA_TRANSP);
    _list_panel->setScrollbarMode(LV_SCROLLBAR_MODE_AUTO);
    _list_panel->setFlexFlow(LV_FLEX_FLOW_COLUMN);
    _list_panel->setFlexAlign(LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    _list_panel->addEventCb(handleLongPressed, LV_EVENT_LONG_PRESSED, this);

    _empty_hint_label = std::make_unique<Label>(_panel->get());
    _empty_hint_label->setText("No todos yet\nTap and hold to manage");
    _empty_hint_label->setTextFont(&lv_font_montserrat_18);
    _empty_hint_label->setTextColor(lv_color_hex(_color_hint));
    _empty_hint_label->setWidth(300);
    _empty_hint_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _empty_hint_label->align(LV_ALIGN_CENTER, 0, 20);
    _empty_hint_label->setHidden(true);
}

void TodoView::setItems(const std::vector<todo::storage::TodoItem>& items)
{
    _row_labels.clear();
    _row_checkboxes.clear();
    _row_panels.clear();

    for (const auto& item : items) {
        auto row = std::make_unique<Container>(_list_panel->get());
        row->setSize(LV_PCT(100), _row_height);
        row->setRadius(0);
        row->setBorderWidth(0);
        row->setPaddingAll(0);
        row->setBgOpa(LV_OPA_TRANSP);
        row->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

        auto box = std::make_unique<Container>(row->get());
        box->setSize(_box_size, _box_size);
        box->setRadius(5);
        box->setBorderWidth(2);
        box->setBorderColor(lv_color_hex(_color_box_border));
        box->setBgColor(item.completed ? lv_color_hex(_color_box_done) : lv_color_hex(_color_panel_bg));
        box->setBgOpa(LV_OPA_COVER);
        box->align(LV_ALIGN_LEFT_MID, _box_x, 0);

        auto label = std::make_unique<Label>(row->get());
        label->setText(item.text);
        label->setTextFont(&lv_font_montserrat_18);
        label->setTextColor(item.completed ? lv_color_hex(_color_row_done) : lv_color_hex(_color_row_text));
        lv_obj_set_style_text_decor(label->get(), item.completed ? LV_TEXT_DECOR_STRIKETHROUGH : LV_TEXT_DECOR_NONE,
                                    LV_PART_MAIN);
        label->setWidth(410 - _label_x - 12);
        label->align(LV_ALIGN_LEFT_MID, _label_x, 0);

        const uint32_t id = item.id;
        row->onClick().connect([this, id]() { _pending_toggle_id = id; });

        _row_checkboxes.push_back(std::move(box));
        _row_labels.push_back(std::move(label));
        _row_panels.push_back(std::move(row));
    }

    _empty_hint_label->setHidden(!items.empty());
}

void TodoView::update()
{
    if (!_manage_dialog) {
        return;
    }

    if (_manage_dialog->isConfirmed()) {
        _manage_requested = true;
        _manage_dialog.reset();
        return;
    }

    if (_manage_dialog->isCancelled()) {
        _manage_dialog.reset();
    }
}

uint32_t TodoView::consumeToggleRequest()
{
    const uint32_t id  = _pending_toggle_id;
    _pending_toggle_id = 0;
    return id;
}

bool TodoView::consumeManageRequested()
{
    const bool requested = _manage_requested;
    _manage_requested     = false;
    return requested;
}

void TodoView::showManageDialog()
{
    _manage_dialog.reset();
    _manage_dialog = std::make_unique<TodoManageDialog>();
    _manage_dialog->init(lv_screen_active());
}

void TodoView::handleLongPressed(lv_event_t* e)
{
    auto* self = static_cast<TodoView*>(lv_event_get_user_data(e));
    if (self == nullptr) {
        return;
    }
    self->showManageDialog();
}
