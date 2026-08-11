/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_pendulum.h"

#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>

using namespace mooncake;

AppPendulum::AppPendulum()
{
    setAppInfo().name = "Pendulum";
    setAppInfo().icon = (void*)&icon_pendulum_app;
}

void AppPendulum::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppPendulum::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();

    LvglLockGuard lock;
    _view = std::make_unique<view::PendulumView>();
    _view->init(lv_screen_active());
}

void AppPendulum::onRunning()
{
    input::KeyEvent event = input::KeyEvent::None;
    if (_key_manager) {
        event = _key_manager->update();
    }

    if (event == input::KeyEvent::GoHome) {
        close();
        return;
    }
}

void AppPendulum::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();

    LvglLockGuard lock;
    _view.reset();
}

void AppPendulum::resetPendulum()
{
    _theta_rad   = 0.0;
    _omega_rad_s = 0.0;
}

void AppPendulum::stepPhysics(double /*dtSeconds*/)
{
    // Implemented in Task 5.
}
