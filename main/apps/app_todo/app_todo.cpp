/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_todo.h"
#include "net/todo_server.h"
#include "storage/todo_storage.h"

#include <apps/common/loading_page/loading_page.h>
#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>

using namespace mooncake;

AppTodo::AppTodo()
{
    setAppInfo().name = "Todo";
    setAppInfo().icon = (void*)&icon_todo_app;
}

void AppTodo::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppTodo::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();

    todo::storage::ensure_dir();

    LvglLockGuard lock;
    _view = std::make_unique<view::TodoView>();
    _view->init(lv_screen_active());
    reloadItems();
}

void AppTodo::onRunning()
{
    input::KeyEvent event = input::KeyEvent::None;
    if (_key_manager) {
        event = _key_manager->update();
    }

    if (event == input::KeyEvent::GoHome) {
        close();
        return;
    }

    if (!_view) {
        return;
    }

    LvglLockGuard lock;

    const uint32_t toggled_id = _view->consumeToggleRequest();
    if (toggled_id != 0) {
        todo::storage::toggle_item(toggled_id);
        reloadItems();
    }

    _view->update();

    if (_view->consumeManageRequested()) {
        mclog::tagInfo(getAppInfo().name, "start management mode");

        auto loading_page = std::make_unique<view::LoadingPage>(0x000000, 0xFFFFFF);
        loading_page->setMessage("Starting management mode...");
        _view.reset();

        GetHAL().lvglUnlock();
        todo::net::run_management_mode([&](std::string_view message) {
            LvglLockGuard log_lock;
            loading_page->setMessage(message);
        });
        GetHAL().lvglLock();

        loading_page.reset();
        _view = std::make_unique<view::TodoView>();
        _view->init(lv_screen_active());
        reloadItems();
    }
}

void AppTodo::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();

    LvglLockGuard lock;
    _view.reset();
}

void AppTodo::reloadItems()
{
    const auto items = todo::storage::load_items();
    if (_view) {
        _view->setItems(items);
    }
}
