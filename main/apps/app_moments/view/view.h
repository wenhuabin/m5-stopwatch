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

class MomentsView {
public:
    void init(lv_obj_t* parent = lv_screen_active());

    // Rebuilds the horizontally swipeable photo strip (or shows the
    // empty-state hint if `photoPaths` is empty), and resets scroll
    // position back to the first photo.
    void setPhotos(const std::vector<std::string>& photoPaths);

    // Runs pending dialog state transitions. Call once per app loop
    // iteration.
    void update();

    // Consumes (and clears) a pending "user confirmed upload" request.
    bool consumeUploadRequested();

private:
    void showUploadDialog();
    static void handleLongPressed(lv_event_t* e);

    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Image>> _photo_images;
    std::unique_ptr<uitk::lvgl_cpp::Label> _empty_hint_label;
    std::unique_ptr<MomentsUploadDialog> _upload_dialog;

    std::vector<std::string> _photo_paths;
    bool _upload_requested = false;
};

}  // namespace view
