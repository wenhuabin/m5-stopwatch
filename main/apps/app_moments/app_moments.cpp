/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_moments.h"
#include "storage/moments_storage.h"

#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>

using namespace mooncake;

AppMoments::AppMoments()
{
    setAppInfo().name = "Moments";
    setAppInfo().icon = (void*)&icon_moments;
}

void AppMoments::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppMoments::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();

    moments::storage::ensure_dir();

    LvglLockGuard lock;
    _view = std::make_unique<view::MomentsView>();
    _view->init(lv_screen_active());
    reloadPhotos();
}

void AppMoments::onRunning()
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

    const view::TapSide tap = _view->consumeTap();
    if (tap == view::TapSide::Left) {
        _view->showPrevious();
    } else if (tap == view::TapSide::Right) {
        _view->showNext();
    }

    _view->update();
}

void AppMoments::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();

    LvglLockGuard lock;
    _view.reset();
}

void AppMoments::reloadPhotos()
{
    const auto photos = moments::storage::load_existing_photos();
    if (_view) {
        _view->setPhotos(photos);
    }
}
