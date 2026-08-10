/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <functional>
#include <string_view>

namespace todo::net {

// Starts a Wi-Fi AP hotspot and an HTTP server serving the todo
// management page (add/edit/delete). Blocks until the phone-side "Done"
// button (POST /close) is used, then tears the AP/server down and
// returns. `onLog` is invoked with human-readable status text suitable
// for display on the watch (SSID, errors).
void run_management_mode(const std::function<void(std::string_view)>& onLog);

}  // namespace todo::net
