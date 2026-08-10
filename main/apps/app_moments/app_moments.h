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

class AppMoments : public mooncake::AppAbility {
public:
    AppMoments();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    void reloadPhotos();

    std::unique_ptr<input::KeyManager> _key_manager;
    std::unique_ptr<view::MomentsView> _view;
};
