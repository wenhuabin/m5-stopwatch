/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "view/view.h"
#include <apps/common/key_manager/key_manager.h>
#include <memory>
#include <mooncake.h>

class AppTodo : public mooncake::AppAbility {
public:
    AppTodo();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    void reloadItems();

    std::unique_ptr<input::KeyManager> _key_manager;
    std::unique_ptr<view::TodoView> _view;
};
