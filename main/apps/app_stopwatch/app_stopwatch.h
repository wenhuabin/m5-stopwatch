/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "view/view.h"
#include <apps/common/key_manager/key_manager.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mooncake.h>

class AppStopwatch : public mooncake::AppAbility {
public:
    AppStopwatch();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    // BtnA: idle -> running -> paused -> idle (reset).
    void advancePhase();
    // BtnB: records a lap while running.
    void recordLap();
    void refreshBatteryIfDue();

    std::unique_ptr<input::KeyManager> _key_manager;
    std::unique_ptr<view::StopwatchView> _view;

    view::StopwatchPhase _phase   = view::StopwatchPhase::Idle;
    uint32_t _start_tick_ms       = 0;
    uint32_t _elapsed_at_pause_ms = 0;

    std::size_t _lap_count        = 0;
    uint32_t _last_lap_elapsed_ms = 0;
    uint32_t _best_lap_ms         = 0;
    uint32_t _worst_lap_ms        = 0;
    bool _has_lap                 = false;

    uint32_t _last_battery_check_ms = 0;
};
