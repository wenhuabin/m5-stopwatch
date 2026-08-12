/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_settings.h"
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <assets/assets.h>

using namespace mooncake;
using namespace view;
using namespace setup_workers;

AppSettings::AppSettings()
{
    setAppInfo().name = "Settings";
    setAppInfo().icon = (void*)&icon_settings_app;
}

void AppSettings::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppSettings::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();

    _destroy_menu    = false;
    _need_warm_reset = false;
    _magic_count     = 0;

    _menu_sections = {
        {
            "Device",
            {
                {"Brightness",
                 [&]() {
                     _destroy_menu = true;
                     _worker       = std::make_unique<BrightnessWorker>();
                 }},
                {"Volume",
                 [&]() {
                     _destroy_menu = true;
                     _worker       = std::make_unique<VolumeWorker>();
                 }},
                {"Button",
                 [&]() {
                     _destroy_menu = true;
                     _worker       = std::make_unique<ButtonWorker>();
                 }},
            },
        },
        {
            "Time & Date",
            {
                {"Set Time",
                 [&]() {
                     _destroy_menu = true;
                     _worker       = std::make_unique<SetTimeWorker>();
                 }},
                {"Set Date",
                 [&]() {
                     _destroy_menu = true;
                     _worker       = std::make_unique<SetDateWorker>();
                 }},
            },
        },
        {
            "Firmware",
            {
                {fmt::format("Version: {}", common::FirmwareVersion), [&]() {}},
            },
        },
    };

    LvglLockGuard lock;

    _menu_page = std::make_unique<view::SelectMenuPage>(_menu_sections);
}

void AppSettings::onRunning()
{
    if (_key_manager && _key_manager->update() == input::KeyEvent::GoHome) {
        close();
        return;
    }

    LvglLockGuard lock;

    if (_menu_page) {
        _menu_page->update();
    }

    if (_destroy_menu) {
        _menu_page.reset();
        _destroy_menu = false;
    }

    if (_worker) {
        _worker->update();
        if (_worker->isDone()) {
            _worker.reset();
            _menu_page = std::make_unique<view::SelectMenuPage>(_menu_sections);
        }
    }
}

void AppSettings::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();

    LvglLockGuard lock;

    _menu_sections.clear();
    _menu_page.reset();
    _worker.reset();

    if (_need_warm_reset) {
        // GetHAL().requestWarmReboot(6);
    }
}
