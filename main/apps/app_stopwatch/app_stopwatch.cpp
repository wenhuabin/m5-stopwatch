/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_stopwatch.h"

#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>

using namespace mooncake;

AppStopwatch::AppStopwatch()
{
    setAppInfo().name = "Stopwatch";
    setAppInfo().icon = (void*)&icon_stopwatch_app;
}

void AppStopwatch::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppStopwatch::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();

    _phase                = view::StopwatchPhase::Idle;
    _start_tick_ms        = 0;
    _elapsed_at_pause_ms  = 0;
    _lap_count            = 0;
    _last_lap_elapsed_ms  = 0;
    _has_lap               = false;

    _last_battery_check_ms = 0;

    LvglLockGuard lock;
    _view = std::make_unique<view::StopwatchView>();
    _view->init(lv_screen_active());
    _view->setElapsed(0);
    _view->setBatteryLevel(GetHAL().getBatteryLevel(), GetHAL().isBatteryCharging());
}

void AppStopwatch::onRunning()
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
        advancePhase();
    } else if (event == input::KeyEvent::GoNext) {
        recordLap();
    }

    uint32_t elapsed = 0;
    if (_phase == view::StopwatchPhase::Running) {
        elapsed = GetHAL().millis() - _start_tick_ms;
    } else if (_phase == view::StopwatchPhase::Paused) {
        elapsed = _elapsed_at_pause_ms;
    }
    _view->setElapsed(elapsed);

    refreshBatteryIfDue();
}

void AppStopwatch::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();

    LvglLockGuard lock;
    _view.reset();
}

void AppStopwatch::advancePhase()
{
    switch (_phase) {
        case view::StopwatchPhase::Idle:
            _start_tick_ms = GetHAL().millis();
            _phase         = view::StopwatchPhase::Running;
            break;

        case view::StopwatchPhase::Running:
            _elapsed_at_pause_ms = GetHAL().millis() - _start_tick_ms;
            _phase               = view::StopwatchPhase::Paused;
            break;

        case view::StopwatchPhase::Paused:
            _phase               = view::StopwatchPhase::Idle;
            _elapsed_at_pause_ms = 0;
            _lap_count           = 0;
            _last_lap_elapsed_ms = 0;
            _has_lap             = false;
            _view->resetLaps();
            _view->setElapsed(0);
            GetHAL().vibrate(150, 100);
            break;
    }

    _view->setPhase(_phase);
}

void AppStopwatch::recordLap()
{
    if (_phase != view::StopwatchPhase::Running) {
        return;
    }

    const uint32_t now_elapsed = GetHAL().millis() - _start_tick_ms;
    const uint32_t lap_ms      = now_elapsed - _last_lap_elapsed_ms;
    _last_lap_elapsed_ms       = now_elapsed;
    _lap_count += 1;

    const bool is_best  = !_has_lap || lap_ms < _best_lap_ms;
    const bool is_worst = !_has_lap || lap_ms > _worst_lap_ms;
    if (is_best) {
        _best_lap_ms = lap_ms;
    }
    if (is_worst) {
        _worst_lap_ms = lap_ms;
    }
    _has_lap = true;

    _view->addLap(_lap_count, lap_ms, is_best, is_worst);
}

void AppStopwatch::refreshBatteryIfDue()
{
    constexpr uint32_t kBatteryCheckIntervalMs = 20000;

    const uint32_t now = GetHAL().millis();
    if (now - _last_battery_check_ms < kBatteryCheckIntervalMs) {
        return;
    }
    _last_battery_check_ms = now;
    _view->setBatteryLevel(GetHAL().getBatteryLevel(), GetHAL().isBatteryCharging());
}
