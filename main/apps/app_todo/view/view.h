/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "../storage/todo_storage.h"
#include <cstdint>
#include <memory>
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <vector>

namespace view {

class TodoManageDialog {
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

// White-background checklist: a scrollable column of rows (checkbox +
// text, struck through once completed). Long-press anywhere opens a
// dialog to start the phone-browser management page (add/edit/delete).
class TodoView {
public:
    void init(lv_obj_t* parent = lv_screen_active());

    // Rebuilds the row list from `items` (already sorted for display by
    // the storage layer).
    void setItems(const std::vector<todo::storage::TodoItem>& items);

    // Runs pending dialog state transitions. Call once per app loop
    // iteration.
    void update();

    // Consumes (and clears) the id of a row the user tapped to toggle,
    // or 0 if none is pending.
    uint32_t consumeToggleRequest();

    // Consumes (and clears) a pending "user confirmed management mode"
    // request.
    bool consumeManageRequested();

private:
    void showManageDialog();
    static void handleLongPressed(lv_event_t* e);

    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _title_label;
    std::unique_ptr<uitk::lvgl_cpp::Container> _list_panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _empty_hint_label;

    std::vector<std::unique_ptr<uitk::lvgl_cpp::Container>> _row_panels;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Container>> _row_checkboxes;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Label>> _row_labels;

    std::unique_ptr<TodoManageDialog> _manage_dialog;

    uint32_t _pending_toggle_id = 0;
    bool _manage_requested       = false;
};

}  // namespace view
