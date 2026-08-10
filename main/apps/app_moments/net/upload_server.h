/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <functional>
#include <string_view>

namespace moments::net {

// Starts a Wi-Fi AP hotspot and an HTTP server that serves the upload
// page and accepts photo batches. Blocks until the phone-side "Done"
// button (POST /close) is used, then tears the AP/server down and
// returns. `onLog` is invoked with human-readable status text suitable
// for display on the watch (SSID, upload progress, errors).
void run_upload_mode(const std::function<void(std::string_view)>& onLog);

}  // namespace moments::net
