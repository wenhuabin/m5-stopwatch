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

constexpr int _panel_size           = 466;
constexpr int _hint_width           = 320;
constexpr int _dialog_width         = 404;
constexpr int _dialog_height        = 202;
constexpr int _dialog_button_width  = 148;
constexpr int _dialog_button_height = 63;

constexpr uint32_t _dialog_bg_color      = 0x2B2B2B;
constexpr uint32_t _dialog_border_color  = 0x6A6A6A;
constexpr uint32_t _dialog_confirm_color = 0x5865F2;
constexpr uint32_t _dialog_cancel_color  = 0x515151;
constexpr uint32_t _label_color          = 0xFFFFFF;
constexpr uint32_t _hint_text_color      = 0xD9D9D9;

}  // namespace

void MomentsUploadDialog::init(lv_obj_t* parent)
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
    _label->setText("Upload new photos?");
    _label->setTextFont(&MontserratSemiBold26);
    _label->setTextColor(lv_color_hex(_label_color));
    _label->align(LV_ALIGN_TOP_LEFT, 49, 40);

    _confirm_button = std::make_unique<Button>(_panel->get());
    _confirm_button->setSize(_dialog_button_width, _dialog_button_height);
    _confirm_button->align(LV_ALIGN_CENTER, -92, 36);
    _confirm_button->setRadius(LV_RADIUS_CIRCLE);
    _confirm_button->setBorderWidth(0);
    _confirm_button->setShadowWidth(0);
    _confirm_button->setBgColor(lv_color_hex(_dialog_confirm_color));
    _confirm_button->label().setText("Upload");
    _confirm_button->label().setTextFont(&lv_font_montserrat_24);
    _confirm_button->label().setTextColor(lv_color_hex(_label_color));
    _confirm_button->label().align(LV_ALIGN_CENTER, 0, 0);
    _confirm_button->onClick().connect([this]() { _is_confirmed = true; });

    _cancel_button = std::make_unique<Button>(_panel->get());
    _cancel_button->setSize(_dialog_button_width, _dialog_button_height);
    _cancel_button->align(LV_ALIGN_CENTER, 92, 36);
    _cancel_button->setRadius(LV_RADIUS_CIRCLE);
    _cancel_button->setBorderWidth(0);
    _cancel_button->setShadowWidth(0);
    _cancel_button->setBgColor(lv_color_hex(_dialog_cancel_color));
    _cancel_button->label().setText("Cancel");
    _cancel_button->label().setTextFont(&lv_font_montserrat_24);
    _cancel_button->label().setTextColor(lv_color_hex(_label_color));
    _cancel_button->label().align(LV_ALIGN_CENTER, 0, 0);
    _cancel_button->onClick().connect([this]() { _is_cancelled = true; });
}

void MomentsView::init(lv_obj_t* parent)
{
    _photo_paths.clear();
    _photo_index      = 0;
    _pending_tap      = TapSide::None;
    _upload_requested = false;

    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_panel_size, _panel_size);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->setBgColor(lv_color_black());
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _panel->addEventCb(handleClicked, LV_EVENT_CLICKED, this);
    _panel->addEventCb(handleLongPressed, LV_EVENT_LONG_PRESSED, this);

    _image = std::make_unique<Image>(_panel->get());
    _image->align(LV_ALIGN_CENTER, 0, 0);
    _image->setHidden(true);

    _empty_hint_label = std::make_unique<Label>(_panel->get());
    _empty_hint_label->setText("No photos yet\nTap and hold to upload");
    _empty_hint_label->setTextFont(&lv_font_montserrat_24);
    _empty_hint_label->setTextColor(lv_color_hex(_hint_text_color));
    _empty_hint_label->setWidth(_hint_width);
    _empty_hint_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _empty_hint_label->align(LV_ALIGN_CENTER, 0, 0);
    _empty_hint_label->setHidden(true);
}

void MomentsView::setPhotos(const std::vector<std::string>& photoPaths)
{
    _photo_paths = photoPaths;
    showPhoto(0);
}

void MomentsView::showPhoto(std::size_t index)
{
    if (_photo_paths.empty()) {
        if (_image) {
            _image->setHidden(true);
        }
        if (_empty_hint_label) {
            _empty_hint_label->setHidden(false);
        }
        return;
    }

    _photo_index = index % _photo_paths.size();
    const std::string lvgl_path = "A:" + _photo_paths[_photo_index];
    lv_image_set_src(_image->get(), lvgl_path.c_str());
    _image->setHidden(false);
    if (_empty_hint_label) {
        _empty_hint_label->setHidden(true);
    }
}

void MomentsView::showNext()
{
    if (_photo_paths.size() < 2) {
        return;
    }
    showPhoto(_photo_index + 1);
}

void MomentsView::showPrevious()
{
    if (_photo_paths.size() < 2) {
        return;
    }
    showPhoto((_photo_index + _photo_paths.size() - 1) % _photo_paths.size());
}

void MomentsView::update()
{
    if (!_upload_dialog) {
        return;
    }

    if (_upload_dialog->isConfirmed()) {
        _upload_requested = true;
        _upload_dialog.reset();
        return;
    }

    if (_upload_dialog->isCancelled()) {
        _upload_dialog.reset();
    }
}

TapSide MomentsView::consumeTap()
{
    const TapSide tap = _pending_tap;
    _pending_tap       = TapSide::None;
    return tap;
}

bool MomentsView::consumeUploadRequested()
{
    const bool requested = _upload_requested;
    _upload_requested     = false;
    return requested;
}

void MomentsView::showUploadDialog()
{
    _upload_dialog.reset();
    _upload_dialog = std::make_unique<MomentsUploadDialog>();
    _upload_dialog->init(lv_screen_active());
}

void MomentsView::handleClicked(lv_event_t* e)
{
    auto* self = static_cast<MomentsView*>(lv_event_get_user_data(e));
    if (self == nullptr || self->_photo_paths.size() < 2) {
        return;
    }

    lv_indev_t* indev = lv_event_get_indev(e);
    if (indev == nullptr) {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    self->_pending_tap = (point.x < _panel_size / 2) ? TapSide::Left : TapSide::Right;
}

void MomentsView::handleLongPressed(lv_event_t* e)
{
    auto* self = static_cast<MomentsView*>(lv_event_get_user_data(e));
    if (self == nullptr) {
        return;
    }
    self->showUploadDialog();
}
