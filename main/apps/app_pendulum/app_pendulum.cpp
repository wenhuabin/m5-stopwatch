/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_pendulum.h"

#include <assets/assets.h>
#include <cmath>
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>

using namespace mooncake;

namespace {
constexpr double kOmega0Sq        = 6.8;   // rad^2/s^2 -- tuned for a ~2.4s period
constexpr double kDamping         = 0.15;  // 1/s -- tuned for a ~15-20s settle time
constexpr double kDefaultAngleRad = 45.0 * 3.14159265358979323846 / 180.0;
constexpr uint32_t kMaxDtMs       = 50;
}  // namespace

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

    _theta_rad      = kDefaultAngleRad;
    _last_update_ms = GetHAL().millis();
    _view->setAngle(_theta_rad);
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

    if (!_view) {
        return;
    }

    LvglLockGuard lock;

    if (event == input::KeyEvent::GoPrevious) {
        resetPendulum();
    }

    const uint32_t now = GetHAL().millis();
    uint32_t dt_ms      = now - _last_update_ms;
    _last_update_ms      = now;
    if (dt_ms > kMaxDtMs) {
        dt_ms = kMaxDtMs;
    }

    if (_view->isDragging()) {
        _theta_rad   = _view->dragAngleRad();
        _omega_rad_s = 0.0;
    } else {
        if (_view->consumeReleaseRequested()) {
            _omega_rad_s = 0.0;
        }
        stepPhysics(dt_ms / 1000.0);
    }

    _view->setAngle(_theta_rad);
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
    _theta_rad   = kDefaultAngleRad;
    _omega_rad_s = 0.0;
}

void AppPendulum::stepPhysics(double dtSeconds)
{
    const double angular_accel = -kOmega0Sq * std::sin(_theta_rad) - kDamping * _omega_rad_s;
    _omega_rad_s += angular_accel * dtSeconds;
    _theta_rad += _omega_rad_s * dtSeconds;
}
