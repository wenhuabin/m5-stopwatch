/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "view/view.h"
#include <apps/common/key_manager/key_manager.h>
#include <cstdint>
#include <memory>
#include <mooncake.h>

class AppPendulum : public mooncake::AppAbility {
public:
    AppPendulum();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    // BtnA: snap back to the default angle and restart the swing.
    void resetPendulum();
    // Advances the damped-pendulum ODE by dtSeconds (semi-implicit Euler).
    void stepPhysics(double dtSeconds);

    std::unique_ptr<input::KeyManager> _key_manager;
    std::unique_ptr<view::PendulumView> _view;

    double _theta_rad        = 0.0;
    double _omega_rad_s      = 0.0;
    uint32_t _last_update_ms = 0;
};
