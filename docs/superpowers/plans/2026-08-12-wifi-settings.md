# On-Device Wi-Fi Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a Wi-Fi settings screen to the Settings app: scan nearby
networks on the device's own screen, tap one, type the password on an
on-screen keyboard, connect. Auto-reconnect to a previously configured
network on every boot.

**Architecture:** `main.cpp` initializes the already-linked-but-unused
`78/esp-wifi-connect` component's `WifiManager` singleton at boot. A new
`WifiWorker` (in `main/apps/app_settings/workers/wifi.cpp`, declared in the
existing `workers.h`) owns a background FreeRTOS task that runs ESP-IDF's
blocking `esp_wifi_scan_start()` off the main thread, handing results back
through an `std::atomic<bool>` flag that `WifiWorker::update()` polls every
`onRunning()` tick — the same "background work never touches LVGL, the main
loop polls a flag" discipline already used by `KeyManager`, Todo's toggle
requests, and Pendulum's drag state.

## Global Constraints

- No automated test framework for this embedded/LVGL codebase —
  verification is build-clean + flash-and-manually-check on the physical
  device at `/dev/cu.usbmodem1101`, same as every prior feature in this
  project.
- Source ESP-IDF before any `idf.py` command: `. ~/esp/esp-idf/export.sh`.
- New `.cpp` files need `idf.py reconfigure` before the next build.
- Check `lsof /dev/cu.usbmodem1101` and kill any leftover listener before
  flashing.
- `#include <wifi_manager.h>` / `<ssid_manager.h>` work directly with no
  path prefix — confirmed via
  `managed_components/78__esp-wifi-connect/CMakeLists.txt`
  (`INCLUDE_DIRS "include"`), and `main/idf_component.yml` already declares
  `78/esp-wifi-connect: 3.1.3` as a dependency, so `main`'s sources get the
  include path automatically — no `main/CMakeLists.txt` changes needed.
- Background FreeRTOS tasks must never call LVGL functions directly (no
  `LvglLockGuard`, no widget access) — only ever write to plain data
  members, guarded by the `std::atomic<bool>` handoff flag described above.
- Commit after each task.

---

### Task 1: Initialize WifiManager and auto-reconnect at boot

**Files:**
- Modify: `main/main.cpp`

**Interfaces:**
- Produces: `WifiManager::GetInstance()` is initialized and station mode
  started before the main loop begins — available to any app from this
  point on, though nothing consumes it yet until Task 2.

- [ ] **Step 1: Add the include and boot-time init** — in `main/main.cpp`,
  add near the top:

```cpp
#include <wifi_manager.h>
```

In `app_main()`, right after `GetHAL().init();`:

```cpp
    // WiFi (station mode, auto-reconnect using any previously configured
    // network; Settings -> Network -> Wi-Fi lets the user add one)
    WifiManager::GetInstance().Initialize();
    WifiManager::GetInstance().StartStation();
```

- [ ] **Step 2: Reconfigure and build**

```bash
. ~/esp/esp-idf/export.sh
idf.py reconfigure
idf.py build
```

Expected: builds cleanly.

- [ ] **Step 3: Flash and verify via serial log**

```bash
lsof /dev/cu.usbmodem1101
idf.py -p /dev/cu.usbmodem1101 flash
```

Capture a serial log across a reboot (reuse this session's established
pyserial-logger pattern) and confirm no crash/panic during boot. If a
network was never configured, expect no WiFi-related errors (just silent
no-op); this can't show a *successful* reconnect yet since nothing has
been configured through the device — that's verified end-to-end in Task 3.

- [ ] **Step 4: Commit**

```bash
git add main/main.cpp
git commit -m "Initialize WifiManager and auto-reconnect at boot"
```

---

### Task 2: Scan + status/list view, reachable from a new Network menu section

**Files:**
- Modify: `main/apps/app_settings/workers/workers.h`
- Create: `main/apps/app_settings/workers/wifi.cpp`
- Modify: `main/apps/app_settings/app_settings.cpp`

**Interfaces:**
- Produces: `WifiWorker` (declared in `workers.h`), showing a status line
  (connected SSID or "Not connected") and a scrollable list of scanned
  networks. Rows aren't tappable yet — Task 3 adds that.

- [ ] **Step 1: Add includes and declare `WifiWorker`** — in
  `main/apps/app_settings/workers/workers.h`, add to the includes at the
  top:

```cpp
#include <atomic>
#include <esp_wifi.h>
#include <vector>
```

Add the class declaration at the end, before the closing
`}  // namespace setup_workers`:

```cpp
/**
 * @brief
 *
 */
class WifiWorker : public WorkerBase {
public:
    WifiWorker();
    ~WifiWorker();
    void update() override;

private:
    class WifiConfigView;

    static void scanTaskEntry(void* param);
    void runScanTask();
    void startScan();

    std::unique_ptr<WifiConfigView> _view;
    std::vector<wifi_ap_record_t> _scan_results;
    std::atomic<bool> _scan_ready{false};
};
```

- [ ] **Step 2: Create `main/apps/app_settings/workers/wifi.cpp`**

```cpp
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "workers.h"
#include <assets/assets.h>
#include <mooncake_log.h>
#include <wifi_manager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdio>

using namespace smooth_ui_toolkit::lvgl_cpp;
using namespace setup_workers;

static const std::string_view _tag = "Setup-Wifi";

class WifiWorker::WifiConfigView {
public:
    WifiConfigView()
    {
        _panel = std::make_unique<Container>(lv_screen_active());
        _panel->align(LV_ALIGN_CENTER, 0, 0);
        _panel->setSize(466, 466);
        _panel->setRadius(0);
        _panel->setBorderWidth(0);
        _panel->setPaddingAll(0);
        _panel->setBgColor(lv_color_hex(0x000000));
        _panel->setBgOpa(LV_OPA_COVER);
        _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

        _title_label = std::make_unique<Label>(_panel->get());
        _title_label->align(LV_ALIGN_TOP_MID, 0, 34);
        _title_label->setText("Wi-Fi");
        _title_label->setTextFont(&MontserratSemiBold26);
        _title_label->setTextColor(lv_color_hex(0xFFFFFF));

        _status_label = std::make_unique<Label>(_panel->get());
        _status_label->align(LV_ALIGN_TOP_MID, 0, 72);
        _status_label->setTextFont(&lv_font_montserrat_18);
        _status_label->setTextColor(lv_color_hex(0x9A9A9A));
        _status_label->setWidth(400);
        _status_label->setTextAlign(LV_TEXT_ALIGN_CENTER);

        _list_panel = std::make_unique<Container>(_panel->get());
        _list_panel->align(LV_ALIGN_CENTER, 0, 10);
        _list_panel->setSize(400, 250);
        _list_panel->setRadius(0);
        _list_panel->setBorderWidth(0);
        _list_panel->setPaddingAll(0);
        _list_panel->setBgOpa(LV_OPA_TRANSP);
        _list_panel->setScrollDir(LV_DIR_VER);
        _list_panel->setScrollbarMode(LV_SCROLLBAR_MODE_ACTIVE);
        _list_panel->setFlexFlow(LV_FLEX_FLOW_COLUMN);
        _list_panel->setFlexAlign(LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        _list_panel->setPadRow(10);

        _done_button = std::make_unique<Button>(_panel->get());
        _done_button->align(LV_ALIGN_BOTTOM_MID, 0, -24);
        _done_button->setSize(200, 68);
        _done_button->setRadius(34);
        _done_button->setBorderWidth(0);
        _done_button->setShadowWidth(0);
        _done_button->setBgColor(lv_color_hex(0x4C4C4C));
        _done_button->label().setText("Done");
        _done_button->label().setTextFont(&lv_font_montserrat_24);
        _done_button->label().setTextColor(lv_color_hex(0xFFFFFF));
        _done_button->label().align(LV_ALIGN_CENTER, 0, 0);
        _done_button->onClick().connect([this]() { _done_requested = true; });
    }

    void setStatusText(const std::string& text)
    {
        _status_label->setText(text);
    }

    void setNetworks(const std::vector<wifi_ap_record_t>& networks)
    {
        _row_buttons.clear();
        _row_labels.clear();

        for (const auto& ap : networks) {
            const std::string ssid(reinterpret_cast<const char*>(ap.ssid));
            if (ssid.empty()) {
                continue;
            }
            const bool secured = ap.authmode != WIFI_AUTH_OPEN;

            auto row = std::make_unique<Button>(_list_panel->get());
            row->setSize(380, 60);
            row->setRadius(30);
            row->setBorderWidth(0);
            row->setShadowWidth(0);
            row->setBgColor(lv_color_hex(0x2C2C2C));

            auto label = std::make_unique<Label>(row->get());
            label->setText(secured ? ssid + "  (secured)" : ssid);
            label->setTextFont(&lv_font_montserrat_20);
            label->setTextColor(lv_color_hex(0xFFFFFF));
            label->align(LV_ALIGN_LEFT_MID, 24, 0);

            _row_labels.push_back(std::move(label));
            _row_buttons.push_back(std::move(row));
        }
    }

    bool consumeDoneRequested()
    {
        const bool requested = _done_requested;
        _done_requested       = false;
        return requested;
    }

private:
    std::unique_ptr<Container> _panel;
    std::unique_ptr<Label> _title_label;
    std::unique_ptr<Label> _status_label;
    std::unique_ptr<Container> _list_panel;
    std::vector<std::unique_ptr<Button>> _row_buttons;
    std::vector<std::unique_ptr<Label>> _row_labels;
    std::unique_ptr<Button> _done_button;
    bool _done_requested = false;
};

WifiWorker::WifiWorker()
{
    mclog::tagInfo(_tag, "start wifi worker");

    _view = std::make_unique<WifiConfigView>();
    startScan();
}

WifiWorker::~WifiWorker()
{
}

void WifiWorker::startScan()
{
    _scan_ready = false;
    _view->setStatusText("Scanning...");
    xTaskCreate(&WifiWorker::scanTaskEntry, "wifi_scan_task", 4096, this, 5, nullptr);
}

void WifiWorker::scanTaskEntry(void* param)
{
    static_cast<WifiWorker*>(param)->runScanTask();
    vTaskDelete(nullptr);
}

void WifiWorker::runScanTask()
{
    esp_wifi_scan_start(nullptr, true);

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    std::vector<wifi_ap_record_t> results(ap_count);
    if (ap_count > 0) {
        esp_wifi_scan_get_ap_records(&ap_count, results.data());
        results.resize(ap_count);
    }

    _scan_results = std::move(results);
    _scan_ready   = true;
}

void WifiWorker::update()
{
    if (_scan_ready.exchange(false)) {
        std::string status;
        if (WifiManager::GetInstance().IsConnected()) {
            status = fmt::format("Connected: {}", WifiManager::GetInstance().GetSsid());
        } else {
            status = "Not connected";
        }
        _view->setStatusText(status);
        _view->setNetworks(_scan_results);
    }

    if (_view->consumeDoneRequested()) {
        _is_done = true;
    }
}
```

> Note: `esp_wifi_scan_start(nullptr, true)` uses ESP-IDF's default scan
> config (all channels, all SSIDs) and blocks *the scan task*, not the main
> app loop — that's the whole point of running it on its own FreeRTOS task.
> `_scan_results` is written by that task and read by `update()` on the
> main task; the `std::atomic<bool> _scan_ready` handoff (default
> sequentially-consistent ordering) makes this a data-race-free handoff —
> the write to `_scan_results` happens-before the `_scan_ready = true`
> store, and `update()`'s `exchange(false)` returning `true` happens-after
> that store, so the vector read afterward is safe.

- [ ] **Step 3: Add the Network menu section** — in
  `main/apps/app_settings/app_settings.cpp`'s `onOpen()`, add a new section
  after `"Time & Date"` and before `"Firmware"`:

```cpp
        {
            "Network",
            {
                {"Wi-Fi",
                 [&]() {
                     _destroy_menu = true;
                     _worker       = std::make_unique<WifiWorker>();
                 }},
            },
        },
```

- [ ] **Step 4: Reconfigure and build**

```bash
idf.py reconfigure
idf.py build
```

Expected: builds cleanly.

- [ ] **Step 5: Flash and verify on hardware**

```bash
lsof /dev/cu.usbmodem1101
idf.py -p /dev/cu.usbmodem1101 flash
```

Open Settings -> Network -> Wi-Fi. Expected: "Scanning..." briefly, then a
status line ("Not connected" most likely) and a scrollable list of real
nearby Wi-Fi networks with "(secured)" suffixed on password-protected
ones. Tapping a row does nothing yet (expected — Task 3). "Done" returns
to the menu.

- [ ] **Step 6: Commit**

```bash
git add main/apps/app_settings
git commit -m "Add Wi-Fi scan/status view, reachable from a new Network menu section"
```

---

### Task 3: Tap to connect (open networks direct, secured via password keyboard)

**Files:**
- Modify: `main/apps/app_settings/workers/wifi.cpp`

**Interfaces:**
- Consumes: `SsidManager::GetInstance().AddSsid(ssid, password)`,
  `WifiManager::GetInstance().StartStation()` (both in the already-linked
  `78/esp-wifi-connect` component).
- Produces: tapping a network row now either connects immediately (open)
  or shows a password-entry screen with an on-screen keyboard (secured),
  whose Connect button triggers the same connection path.

- [ ] **Step 1: Add the include** — in
  `main/apps/app_settings/workers/wifi.cpp`, add:

```cpp
#include <ssid_manager.h>
```

- [ ] **Step 2: Add password-entry widgets and state to `WifiConfigView`**
  — add these members (in the `private:` section, alongside the existing
  ones):

```cpp
    std::unique_ptr<Container> _password_panel;
    std::unique_ptr<Label> _password_title;
    std::unique_ptr<TextArea> _password_area;
    lv_obj_t* _keyboard = nullptr;
    std::unique_ptr<Button> _connect_button;
    std::unique_ptr<Button> _cancel_button;

    std::string _connect_ssid;
    std::string _connect_password;
    bool _connect_requested = false;
```

Add their construction at the end of the constructor (after the
`_done_button` block):

```cpp
        _password_panel = std::make_unique<Container>(_panel->get());
        _password_panel->align(LV_ALIGN_TOP_MID, 0, 100);
        _password_panel->setSize(420, 140);
        _password_panel->setRadius(0);
        _password_panel->setBorderWidth(0);
        _password_panel->setPaddingAll(0);
        _password_panel->setBgOpa(LV_OPA_TRANSP);
        _password_panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
        _password_panel->setHidden(true);

        _password_title = std::make_unique<Label>(_password_panel->get());
        _password_title->align(LV_ALIGN_TOP_MID, 0, 0);
        _password_title->setTextFont(&lv_font_montserrat_20);
        _password_title->setTextColor(lv_color_hex(0xFFFFFF));

        _password_area = std::make_unique<TextArea>(_password_panel->get());
        _password_area->align(LV_ALIGN_TOP_MID, 0, 40);
        _password_area->setSize(400, 60);
        _password_area->setOneLine(true);
        _password_area->setPasswordMode(true);
        _password_area->setPlaceholderText("Password");

        _connect_button = std::make_unique<Button>(_password_panel->get());
        _connect_button->align(LV_ALIGN_TOP_LEFT, 20, 110);
        _connect_button->setSize(180, 60);
        _connect_button->setRadius(30);
        _connect_button->setBorderWidth(0);
        _connect_button->setShadowWidth(0);
        _connect_button->setBgColor(lv_color_hex(0x4AD78C));
        _connect_button->label().setText("Connect");
        _connect_button->label().setTextFont(&lv_font_montserrat_20);
        _connect_button->label().setTextColor(lv_color_hex(0x0F5831));
        _connect_button->label().align(LV_ALIGN_CENTER, 0, 0);
        _connect_button->onClick().connect([this]() {
            _connect_password = lv_textarea_get_text(_password_area->get());
            _connect_requested = true;
            showNetworkList();
        });

        _cancel_button = std::make_unique<Button>(_password_panel->get());
        _cancel_button->align(LV_ALIGN_TOP_RIGHT, -20, 110);
        _cancel_button->setSize(180, 60);
        _cancel_button->setRadius(30);
        _cancel_button->setBorderWidth(0);
        _cancel_button->setShadowWidth(0);
        _cancel_button->setBgColor(lv_color_hex(0x4C4C4C));
        _cancel_button->label().setText("Cancel");
        _cancel_button->label().setTextFont(&lv_font_montserrat_20);
        _cancel_button->label().setTextColor(lv_color_hex(0xFFFFFF));
        _cancel_button->label().align(LV_ALIGN_CENTER, 0, 0);
        _cancel_button->onClick().connect([this]() { showNetworkList(); });

        _keyboard = lv_keyboard_create(_panel->get());
        lv_obj_set_size(_keyboard, 466, 200);
        lv_obj_align(_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_textarea(_keyboard, _password_area->get());
        lv_obj_add_flag(_keyboard, LV_OBJ_FLAG_HIDDEN);
```

(`TextArea` needs `#include`d — add
`#include "smooth_lvgl.hpp"`'s text area header is already pulled in
transitively via `workers.h`'s `<smooth_lvgl.hpp>`, so no new include is
needed for the `TextArea` type itself, only for `<ssid_manager.h>` from
Step 1.)

- [ ] **Step 3: Add the show/hide state-transition helpers and connect
  request accessor** — add these methods to `WifiConfigView` (near
  `setNetworks`):

```cpp
    void showNetworkList()
    {
        _list_panel->setHidden(false);
        _done_button->setHidden(false);
        _password_panel->setHidden(true);
        lv_obj_add_flag(_keyboard, LV_OBJ_FLAG_HIDDEN);
    }

    void showPasswordEntry(const std::string& ssid)
    {
        _connect_ssid = ssid;
        _password_title->setText(fmt::format("Password for {}", ssid));
        _password_area->setText("");
        _list_panel->setHidden(true);
        _done_button->setHidden(true);
        _password_panel->setHidden(false);
        lv_obj_remove_flag(_keyboard, LV_OBJ_FLAG_HIDDEN);
    }

    bool consumeConnectRequested(std::string& outSsid, std::string& outPassword)
    {
        if (!_connect_requested) {
            return false;
        }
        _connect_requested = false;
        outSsid            = _connect_ssid;
        outPassword         = _connect_password;
        return true;
    }
```

- [ ] **Step 4: Wire row taps** — in `setNetworks`, after creating `label`
  and before pushing `label`/`row` into the vectors, add the click handler
  (needs `ssid` and `secured`, already computed above it in the loop):

```cpp
            row->onClick().connect([this, ssid, secured]() {
                if (secured) {
                    showPasswordEntry(ssid);
                } else {
                    _connect_ssid       = ssid;
                    _connect_password   = "";
                    _connect_requested  = true;
                }
            });
```

- [ ] **Step 5: Apply connect requests in `WifiWorker::update()`** — add
  to `WifiWorker::update()`, right after the existing
  `_scan_ready.exchange(false)` block:

```cpp
    std::string connect_ssid;
    std::string connect_password;
    if (_view->consumeConnectRequested(connect_ssid, connect_password)) {
        mclog::tagInfo(_tag, "connecting to {}", connect_ssid);
        SsidManager::GetInstance().AddSsid(connect_ssid, connect_password);
        WifiManager::GetInstance().StartStation();
        _view->setStatusText(fmt::format("Connecting to {}...", connect_ssid));
    }
```

- [ ] **Step 6: Reconfigure and build**

```bash
idf.py reconfigure
idf.py build
```

Expected: builds cleanly.

- [ ] **Step 7: Flash and verify on hardware**

```bash
lsof /dev/cu.usbmodem1101
idf.py -p /dev/cu.usbmodem1101 flash
```

Open Settings -> Network -> Wi-Fi. Tap a secured network: a password field
and on-screen keyboard should appear; type a real password for a network
in range and tap Connect — status should change to "Connecting to
...". Wait a few seconds, back out (Done) and reopen Wi-Fi: status should
now show "Connected: <ssid>" if the password was correct. Also try an open
network (if one's in range) and confirm it connects without a password
prompt. Try Cancel on the password screen and confirm it returns cleanly
to the list without connecting.

- [ ] **Step 8: Commit**

```bash
git add main/apps/app_settings/workers/wifi.cpp
git commit -m "Wire up tap-to-connect (open networks direct, secured via password keyboard)"
```

---

### Task 4: Connecting/result feedback

**Files:**
- Modify: `main/apps/app_settings/workers/workers.h`
- Modify: `main/apps/app_settings/workers/wifi.cpp`

**Interfaces:**
- Produces: after triggering a connect, the view shows "Connecting..."
  then "Connected"/"Failed to connect" (auto-clearing after a few seconds)
  instead of leaving the status text static until the user manually
  reopens the screen.

- [ ] **Step 1: Add connecting-state fields to `WifiWorker`** — in
  `main/apps/app_settings/workers/workers.h`, add two members to
  `WifiWorker`'s `private:` section (after `_scan_ready`):

```cpp
    bool _connecting             = false;
    uint32_t _connect_started_ms = 0;
```

- [ ] **Step 2: Track connection attempts and poll for the result** — in
  `main/apps/app_settings/workers/wifi.cpp`, replace the block added in
  Task 3 Step 5 inside `WifiWorker::update()`:

```cpp
    std::string connect_ssid;
    std::string connect_password;
    if (_view->consumeConnectRequested(connect_ssid, connect_password)) {
        mclog::tagInfo(_tag, "connecting to {}", connect_ssid);
        SsidManager::GetInstance().AddSsid(connect_ssid, connect_password);
        WifiManager::GetInstance().StartStation();
        _view->setStatusText(fmt::format("Connecting to {}...", connect_ssid));
        _connecting          = true;
        _connect_started_ms  = GetHAL().millis();
    }

    if (_connecting) {
        constexpr uint32_t kConnectTimeoutMs = 15000;
        const uint32_t elapsed               = GetHAL().millis() - _connect_started_ms;

        if (WifiManager::GetInstance().IsConnected()) {
            _connecting = false;
            _view->setStatusText(fmt::format("Connected: {}", WifiManager::GetInstance().GetSsid()));
        } else if (elapsed > kConnectTimeoutMs) {
            _connecting = false;
            _view->setStatusText("Failed to connect");
        }
    }
```

(This replaces the simpler version from Task 3 -- the `consumeConnectRequested`
block now also seeds `_connecting`/`_connect_started_ms`, and a new
`if (_connecting)` block below it polls for success/timeout every tick.)

Add `#include <hal/hal.h>` to `wifi.cpp`'s includes (needed for
`GetHAL().millis()` — not yet included in this file from Tasks 2-3).

- [ ] **Step 3: Reconfigure and build**

```bash
idf.py reconfigure
idf.py build
```

Expected: builds cleanly.

- [ ] **Step 4: Flash and verify on hardware**

```bash
lsof /dev/cu.usbmodem1101
idf.py -p /dev/cu.usbmodem1101 flash
```

Open Settings -> Network -> Wi-Fi, connect to a real network (correct
password): confirm the status line progresses "Connecting to X..." ->
"Connected: X" within a few seconds, without needing to leave and reopen
the screen. Try a wrong password (or a network out of range) and confirm
it eventually shows "Failed to connect" instead of hanging on
"Connecting..." forever. Re-verify the rest of Settings (Device, Time &
Date, Firmware easter egg) still works unaffected.

- [ ] **Step 5: Commit**

```bash
git add main/apps/app_settings/workers
git commit -m "Add connecting/result feedback to Wi-Fi settings"
```
