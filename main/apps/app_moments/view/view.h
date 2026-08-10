/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <memory>
#include <smooth_lvgl.hpp>
#include <string>
#include <uitk/short_namespace.hpp>
#include <vector>

namespace view {

class MomentsUploadDialog {
public:
    void init(lv_obj_t* parent);
    bool isConfirmed() const
    {
        return _is_confirmed;
    }
    bool isCancelled() const
    {
        return _is_cancelled;
    }

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _label;
    std::unique_ptr<uitk::lvgl_cpp::Button> _confirm_button;
    std::unique_ptr<uitk::lvgl_cpp::Button> _cancel_button;
    bool _is_confirmed = false;
    bool _is_cancelled = false;
};

enum class TapSide {
    None,
    Left,
    Right,
};

class MomentsView {
public:
    void init(lv_obj_t* parent = lv_screen_active());

    // Replaces the displayed photo list and shows the first photo (or
    // the empty-state hint if `photoPaths` is empty).
    void setPhotos(const std::vector<std::string>& photoPaths);

    void showNext();
    void showPrevious();

    // Runs pending dialog state transitions. Call once per app loop
    // iteration.
    void update();

    // Consumes (and clears) a pending tap-navigation request.
    TapSide consumeTap();

    // Consumes (and clears) a pending "user confirmed upload" request.
    bool consumeUploadRequested();

private:
    void showPhoto(std::size_t index);
    void showUploadDialog();
    static void handleClicked(lv_event_t* e);
    static void handleLongPressed(lv_event_t* e);

    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Image> _image;
    std::unique_ptr<uitk::lvgl_cpp::Label> _empty_hint_label;
    std::unique_ptr<MomentsUploadDialog> _upload_dialog;

    std::vector<std::string> _photo_paths;
    std::size_t _photo_index = 0;
    TapSide _pending_tap     = TapSide::None;
    bool _upload_requested   = false;
};

}  // namespace view
