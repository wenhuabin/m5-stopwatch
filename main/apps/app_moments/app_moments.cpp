/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_moments.h"
#include "net/upload_server.h"
#include "storage/moments_storage.h"

#include <apps/common/loading_page/loading_page.h>
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

    _view->update();

    if (_view->consumeUploadRequested()) {
        mclog::tagInfo(getAppInfo().name, "start upload mode");

        auto loading_page = std::make_unique<view::LoadingPage>(0x000000, 0xFFFFFF);
        loading_page->setMessage("Starting upload mode...");
        _view.reset();

        GetHAL().lvglUnlock();
        moments::net::run_upload_mode([&](std::string_view message) {
            LvglLockGuard log_lock;
            loading_page->setMessage(message);
        });
        GetHAL().lvglLock();

        loading_page.reset();
        _view = std::make_unique<view::MomentsView>();
        _view->init(lv_screen_active());
        reloadPhotos();
    }
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
