/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "workers.h"
#include <assets/assets.h>
#include <mooncake_log.h>
#include <wifi_manager.h>
#include <ssid_manager.h>
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
    }

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

            row->onClick().connect([this, ssid, secured]() {
                if (secured) {
                    showPasswordEntry(ssid);
                } else {
                    _connect_ssid       = ssid;
                    _connect_password   = "";
                    _connect_requested  = true;
                }
            });

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

    std::unique_ptr<Container> _password_panel;
    std::unique_ptr<Label> _password_title;
    std::unique_ptr<TextArea> _password_area;
    lv_obj_t* _keyboard = nullptr;
    std::unique_ptr<Button> _connect_button;
    std::unique_ptr<Button> _cancel_button;

    std::string _connect_ssid;
    std::string _connect_password;
    bool _connect_requested = false;
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

    std::string connect_ssid;
    std::string connect_password;
    if (_view->consumeConnectRequested(connect_ssid, connect_password)) {
        mclog::tagInfo(_tag, "connecting to {}", connect_ssid);
        SsidManager::GetInstance().AddSsid(connect_ssid, connect_password);
        WifiManager::GetInstance().StartStation();
        _view->setStatusText(fmt::format("Connecting to {}...", connect_ssid));
    }

    if (_view->consumeDoneRequested()) {
        _is_done = true;
    }
}
