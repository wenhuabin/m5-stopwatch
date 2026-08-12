/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <hal/hal.h>
#include <atomic>
#include <cstdint>
#include <esp_wifi.h>
#include <memory>
#include <string_view>
#include <vector>

namespace setup_workers {

class PercentageAdjustView;

/**
 * @brief
 *
 */
class WorkerBase {
public:
    virtual ~WorkerBase() = default;

    virtual void update()
    {
    }

    bool isDone() const
    {
        return _is_done;
    }

protected:
    bool _is_done = false;
};

/**
 * @brief
 *
 */
class BrightnessWorker : public WorkerBase {
public:
    BrightnessWorker();
    ~BrightnessWorker();
    void update() override;

private:
    std::unique_ptr<PercentageAdjustView> _view;
    int _applied_brightness = 0;
    bool _save_requested    = false;
};

/**
 * @brief
 *
 */
class VolumeWorker : public WorkerBase {
public:
    VolumeWorker();
    ~VolumeWorker();
    void update() override;

private:
    std::unique_ptr<PercentageAdjustView> _view;
    int _applied_volume  = 0;
    bool _save_requested = false;
};

/**
 * @brief
 *
 */
class ButtonWorker : public WorkerBase {
public:
    ButtonWorker();
    ~ButtonWorker();
    void update() override;

private:
    class ButtonConfigView;

    std::unique_ptr<ButtonConfigView> _view;
    Hal::ButtonConfig _applied_config;
};

/**
 * @brief
 *
 */
class SetTimeWorker : public WorkerBase {
public:
    SetTimeWorker();
    ~SetTimeWorker();
    void update() override;

private:
    class TimeAdjustView;

    std::unique_ptr<TimeAdjustView> _view;
    TimeHms _applied_time;
};

/**
 * @brief
 *
 */
class SetDateWorker : public WorkerBase {
public:
    SetDateWorker();
    ~SetDateWorker();
    void update() override;

private:
    class DateAdjustView;

    std::unique_ptr<DateAdjustView> _view;
    DateYmd _applied_date;
};

/**
 * @brief
 *
 */
class AboutWorker : public WorkerBase {
public:
    AboutWorker();
    ~AboutWorker();
    void update() override;

private:
    class AboutView;

    std::unique_ptr<AboutView> _view;
    int _progress                = 0;
    uint32_t _next_progress_tick = 0;
    int _pending_burst_steps     = 0;
};

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
    bool _connecting             = false;
    uint32_t _connect_started_ms = 0;
};

}  // namespace setup_workers
