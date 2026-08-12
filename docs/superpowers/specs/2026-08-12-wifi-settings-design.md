# On-Device Wi-Fi Settings Design

## Goal

Add a Wi-Fi settings screen to the Settings app that works entirely on the
device's own touchscreen — no phone required: scan nearby networks, tap one,
type the password on an on-screen keyboard, connect. Auto-reconnect to a
previously configured network on every boot.

## Why this needs new plumbing, not just a menu item

This project already links the `78/esp-wifi-connect` managed component
(`main/idf_component.yml`) but has never used it. Its public API
(`WifiManager`, `SsidManager`) covers connecting to a *known* SSID and its
own phone-facing captive-portal web UI for entering *new* credentials — but
exposes no method to list nearby networks. Confirmed by reading
`wifi_station.h`: `HandleScanResult()` (the only scan-result consumer) is
private, used internally only to find a previously-added SSID during
reconnect, not to hand a scan list back to the caller.

Since the user explicitly wants scan-and-pick on the device itself (not the
phone-web-page flow the component ships with), this means calling
ESP-IDF's own `esp_wifi_scan_start()` / `esp_wifi_scan_get_ap_records()`
directly, then feeding a selected SSID + typed password into the
component's `SsidManager::AddSsid()` + `WifiManager::StartStation()` to
actually connect.

## Architecture

- `main.cpp`: after `GetHAL().init()`, add
  `WifiManager::GetInstance().Initialize(); WifiManager::GetInstance().StartStation();`
  once at boot. `StartStation()` reconnects using whatever `SsidManager`
  already has persisted in NVS from a prior session; if nothing is
  persisted, it's a no-op (stays disconnected, doesn't pop up any config
  UI) — this satisfies "auto-reconnect if previously configured."
- `main/apps/app_settings/workers/wifi.cpp` (new): declares/defines
  `WifiWorker : public WorkerBase` (added to `workers.h` alongside the
  existing five workers) and its private `WifiConfigView`.
- `main/apps/app_settings/app_settings.cpp`: new `"Network"` menu section
  with a single `"Wi-Fi"` item constructing `WifiWorker`.

## Scanning: background task, not the main loop

A blocking `esp_wifi_scan_start(nullptr, true)` call takes on the order of
1-3 seconds. Calling it directly from `AppSettings::onRunning()` (the main
app task, which drives *every* app's `onRunning()` in mooncake's single
main loop) would freeze the whole UI for that duration — no redraws, no
other app logic. Instead:

- `WifiWorker`'s constructor spawns a dedicated FreeRTOS task
  (`xTaskCreate`) that runs the blocking scan + `esp_wifi_scan_get_ap_records()`,
  writes the results into a plain `std::vector<wifi_ap_record_t>` member,
  and finally sets a `std::atomic<bool> _scan_ready` flag — written last,
  after the vector is fully populated, so a `memory_order_release` store
  (the default for `std::atomic<bool>::store`) makes the data visible to a
  reader that observes `_scan_ready == true`.
- The scan task **never touches LVGL** — it only fills the results buffer.
- `WifiWorker::update()` (called every `onRunning()` tick, already inside
  `AppSettings`'s `LvglLockGuard`) polls `_scan_ready`; the first time it
  observes `true`, it reads the results vector and builds the on-screen
  list. This mirrors the project's existing discipline of never touching
  LVGL from a background thread and only ever polling flags from the main
  loop (same shape as `KeyManager::update()`, Todo's `_pending_toggle_id`,
  Pendulum's drag-state polling).
- The view shows a "Scanning..." message (reusing `LoadingPage`) until the
  first result set lands.

## View flow (`WifiWorker::WifiConfigView`)

1. **Status line** at the top: `Connected: <ssid> (<rssi> dBm)` /
   `IP: <ip>` if `WifiManager::IsConnected()`, else `Not connected`.
2. **Network list**: one row per scanned SSID (deduplicated, sorted by
   RSSI descending), each row showing the SSID text and a lock glyph when
   `authmode != WIFI_AUTH_OPEN`. A "Rescan" button re-triggers the
   background scan task.
3. **Tap a row**:
   - Open network (`WIFI_AUTH_OPEN`): calls
     `SsidManager::GetInstance().AddSsid(ssid, "")` then
     `WifiManager::GetInstance().StartStation()` immediately.
   - Secured network: switches the view to a password-entry sub-screen —
     a `TextArea` (from `smooth_ui_toolkit`, with `setPasswordMode(true)`
     — confirmed present on the wrapper — so the typed password shows as
     bullets) plus a raw `lv_keyboard_create()` attached via
     `lv_keyboard_set_textarea()` (no `Keyboard` wrapper exists in this
     project yet — this is a first, consistent with how other files mix
     raw LVGL calls alongside wrapper objects where no wrapper exists).
     A "Connect" button submits: `AddSsid(ssid, typed_password)` then
     `StartStation()`.
4. **Connecting feedback**: after triggering `StartStation()`, the view
   shows a "Connecting..." state and polls `WifiManager::IsConnected()`
   (via `WifiWorker::update()`, same polling pattern as scanning) for a
   few seconds, then shows "Connected" or "Failed to connect" and returns
   to the status/list view either way — it doesn't block waiting
   synchronously (`WaitForConnected()` exists on `WifiStation` but isn't
   reachable through `WifiManager`'s public surface, and blocking here
   would have the same main-loop-freeze problem as the scan).
5. **Done/Back button**: always available, returns to the Settings menu
   (matches the `_destroy_menu`/`_worker.reset()` pattern every other
   worker already uses).

## Non-goals

- No "forget this network" / multi-network management UI — `SsidManager`
  supports it (`RemoveSsid`, `SetDefaultSsid`) but the user only asked for
  "can I connect from the device," not full network list management. Can
  be added later if wanted.
- No hidden-SSID (manually typed network name, not from the scan list)
  entry point — out of scope for this request; scanned list only.
- Not touching Moments'/Todo's own AP+HTTP upload/management servers —
  those are a separate, already-working mechanism (device-as-AP for local
  file transfer) unrelated to this device-as-station internet-Wi-Fi
  feature.
