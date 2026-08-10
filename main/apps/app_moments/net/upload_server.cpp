/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "upload_server.h"

#include "../storage/moments_storage.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unistd.h>

#include <dns_server.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <lwip/ip_addr.h>
#include <nvs_flash.h>

#include <hal/hal.h>

namespace moments::net {
namespace {

constexpr const char* _tag                = "MomentsUpload";
constexpr const char* _ap_ssid_prefix     = "M5StopWatch";
constexpr const char* _ap_ssid_suffix     = "-Moments";
constexpr const char* _ap_url             = "http://192.168.4.1";
constexpr EventBits_t _exit_requested_bit = BIT0;
constexpr std::size_t _max_upload_bytes   = 2 * 1024 * 1024;

extern const char moments_upload_html_start[] asm("_binary_moments_upload_html_start");
extern const char moments_upload_html_end[] asm("_binary_moments_upload_html_end");

constexpr const char* _captive_portal_urls[] = {
    "/hotspot-detect.html",      "/generate_204*", "/mobile/status.php",
    "/check_network_status.txt", "/ncsi.txt",      "/fwlink/",
    "/connectivity-check.html",  "/success.txt",   "/portal.html",
    "/library/test/success.html",
};

bool ensure_wifi_stack_ready()
{
    static std::mutex mutex;
    static bool initialized = false;

    std::lock_guard<std::mutex> lock(mutex);
    if (initialized) {
        return true;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(_tag, "nvs init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(_tag, "netif init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(_tag, "event loop init failed: %s", esp_err_to_name(ret));
        return false;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable         = false;
    ret                    = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(_tag, "wifi init failed: %s", esp_err_to_name(ret));
        return false;
    }

    initialized = true;
    return true;
}

std::string make_ap_ssid()
{
    uint8_t mac[6] = {};
    esp_err_t ret  = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    if (ret != ESP_OK) {
        ESP_LOGW(_tag, "read mac failed: %s", esp_err_to_name(ret));
        return std::string(_ap_ssid_prefix) + _ap_ssid_suffix;
    }

    char ssid[32] = {};
    snprintf(ssid, sizeof(ssid), "%s-%02X%02X%s", _ap_ssid_prefix, mac[4], mac[5], _ap_ssid_suffix);
    return std::string(ssid);
}

class Session {
public:
    explicit Session(const std::function<void(std::string_view)>& onLog) : _on_log(onLog) {}

    void run()
    {
        if (!ensure_wifi_stack_ready()) {
            log("Wi-Fi initialization failed");
            return;
        }

        _event_group = xEventGroupCreate();
        if (_event_group == nullptr) {
            log("Failed to create sync event group");
            return;
        }

        if (!start_access_point() || !start_web_server()) {
            stop();
            return;
        }

        log("Connect to Wi-Fi: " + _ssid + "\nThen open:\n" + std::string(_ap_url));

        xEventGroupWaitBits(_event_group, _exit_requested_bit, pdTRUE, pdFALSE, portMAX_DELAY);

        stop();

        log("Upload finished");
    }

private:
    void log(const std::string& message) const
    {
        ESP_LOGI(_tag, "%s", message.c_str());
        if (_on_log) {
            _on_log(message);
        }
    }

    bool start_access_point()
    {
        static esp_netif_t* ap_netif = nullptr;
        if (ap_netif == nullptr) {
            ap_netif = esp_netif_create_default_wifi_ap();
        }
        if (ap_netif == nullptr) {
            log("Failed to create AP network interface");
            return false;
        }

        esp_netif_ip_info_t ip_info;
        IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
        IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
        IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
        esp_netif_dhcps_stop(ap_netif);
        esp_netif_set_ip_info(ap_netif, &ip_info);
        esp_netif_dhcps_start(ap_netif);

        _dns_server = std::make_unique<DnsServer>();
        _dns_server->Start(ip_info.gw);

        _ssid = make_ap_ssid();

        wifi_config_t wifi_config = {};
        strncpy(reinterpret_cast<char*>(wifi_config.ap.ssid), _ssid.c_str(), sizeof(wifi_config.ap.ssid) - 1);
        wifi_config.ap.ssid_len       = _ssid.size();
        wifi_config.ap.max_connection = 4;
        wifi_config.ap.authmode       = WIFI_AUTH_OPEN;

        esp_err_t ret = esp_wifi_stop();
        if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STARTED && ret != ESP_ERR_WIFI_MODE) {
            ESP_LOGW(_tag, "wifi stop before ap failed: %s", esp_err_to_name(ret));
        }

        ret = esp_wifi_set_mode(WIFI_MODE_AP);
        if (ret != ESP_OK) {
            log("Failed to set AP mode");
            return false;
        }

        ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
        if (ret != ESP_OK) {
            log("Failed to apply AP configuration");
            return false;
        }

        ret = esp_wifi_set_ps(WIFI_PS_NONE);
        if (ret != ESP_OK) {
            log("Failed to disable Wi-Fi power save");
            return false;
        }

        ret = esp_wifi_start();
        if (ret != ESP_OK) {
            log("Failed to start AP");
            return false;
        }

        return true;
    }

    bool start_web_server()
    {
        httpd_config_t config    = HTTPD_DEFAULT_CONFIG();
        config.max_uri_handlers  = 16;
        config.recv_wait_timeout = 15;
        config.send_wait_timeout = 15;
        config.uri_match_fn      = httpd_uri_match_wildcard;
        // handle_upload() has a 4KB streaming buffer plus a deep synchronous
        // call chain into FATFS/wear-levelling/flash read; the 4KB default
        // stack_size overflows and corrupts adjacent heap memory.
        config.stack_size = 10240;

        esp_err_t ret = httpd_start(&_server, &config);
        if (ret != ESP_OK) {
            log("Failed to start web server");
            return false;
        }

        httpd_uri_t index  = {.uri = "/", .method = HTTP_GET, .handler = &Session::handle_index, .user_ctx = this};
        httpd_uri_t status = {
            .uri = "/status", .method = HTTP_GET, .handler = &Session::handle_status, .user_ctx = this};
        httpd_uri_t upload = {
            .uri = "/upload", .method = HTTP_POST, .handler = &Session::handle_upload, .user_ctx = this};
        httpd_uri_t close = {
            .uri = "/close", .method = HTTP_POST, .handler = &Session::handle_close, .user_ctx = this};
        httpd_uri_t captive = {
            .uri = nullptr, .method = HTTP_GET, .handler = &Session::handle_captive_portal, .user_ctx = this};

        esp_err_t reg = httpd_register_uri_handler(_server, &index);
        if (reg == ESP_OK) {
            reg = httpd_register_uri_handler(_server, &status);
        }
        if (reg == ESP_OK) {
            reg = httpd_register_uri_handler(_server, &upload);
        }
        if (reg == ESP_OK) {
            reg = httpd_register_uri_handler(_server, &close);
        }
        if (reg == ESP_OK) {
            for (const auto* url : _captive_portal_urls) {
                captive.uri = url;
                reg         = httpd_register_uri_handler(_server, &captive);
                if (reg != ESP_OK) {
                    break;
                }
            }
        }
        if (reg != ESP_OK) {
            log("Failed to register web routes");
            return false;
        }

        return true;
    }

    void stop()
    {
        if (_server != nullptr) {
            httpd_stop(_server);
            _server = nullptr;
        }
        if (_dns_server) {
            _dns_server->Stop();
            _dns_server.reset();
        }
        esp_err_t ret = esp_wifi_stop();
        if (ret != ESP_OK && ret != ESP_ERR_WIFI_NOT_STARTED && ret != ESP_ERR_WIFI_MODE) {
            ESP_LOGW(_tag, "wifi stop failed: %s", esp_err_to_name(ret));
        }
        if (_event_group != nullptr) {
            vEventGroupDelete(_event_group);
            _event_group = nullptr;
        }
    }

    static Session* self_from_request(httpd_req_t* req)
    {
        return static_cast<Session*>(req->user_ctx);
    }

    static bool query_flag_set(httpd_req_t* req, const char* key)
    {
        char query[128] = {};
        if (httpd_req_get_url_query_len(req) <= 0 ||
            httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
            return false;
        }
        char value[8] = {};
        return httpd_query_key_value(query, key, value, sizeof(value)) == ESP_OK && std::string(value) == "1";
    }

    static std::string query_value(httpd_req_t* req, const char* key)
    {
        char query[128] = {};
        if (httpd_req_get_url_query_len(req) <= 0 ||
            httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
            return {};
        }
        char value[64] = {};
        if (httpd_query_key_value(query, key, value, sizeof(value)) != ESP_OK) {
            return {};
        }
        return std::string(value);
    }

    static void send_json(httpd_req_t* req, const std::string& body)
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, body.c_str(), body.size());
    }

    static void send_json_error(httpd_req_t* req, const char* status, const std::string& message)
    {
        httpd_resp_set_status(req, status);
        send_json(req, "{\"error\":\"" + message + "\"}");
    }

    static esp_err_t handle_index(httpd_req_t* req)
    {
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        httpd_resp_send(req, moments_upload_html_start, moments_upload_html_end - moments_upload_html_start);
        return ESP_OK;
    }

    static esp_err_t handle_status(httpd_req_t* req)
    {
        const auto count        = moments::storage::load_existing_photos().size();
        const std::string body = "{\"photos\":" + std::to_string(count) +
                                  ",\"max_photos\":" + std::to_string(moments::storage::kMaxPhotos) +
                                  ",\"max_file_bytes\":" + std::to_string(_max_upload_bytes) + "}";
        send_json(req, body);
        return ESP_OK;
    }

    static esp_err_t handle_close(httpd_req_t* req)
    {
        auto* self = self_from_request(req);
        if (self != nullptr && self->_event_group != nullptr) {
            xEventGroupSetBits(self->_event_group, _exit_requested_bit);
        }
        httpd_resp_sendstr(req, "closing");
        return ESP_OK;
    }

    static esp_err_t handle_captive_portal(httpd_req_t* req)
    {
        const std::string url = std::string(_ap_url) + "/?_=" + std::to_string(esp_timer_get_time());
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", url.c_str());
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send(req, nullptr, 0);
        return ESP_OK;
    }

    static esp_err_t handle_upload(httpd_req_t* req)
    {
        auto* self = self_from_request(req);
        if (self == nullptr) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "server error");
            return ESP_FAIL;
        }

        char content_type[64] = {};
        httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type));
        if (std::string(content_type) != "image/bmp") {
            send_json_error(req, "415 Unsupported Media Type", "only bmp images are supported");
            return ESP_FAIL;
        }

        if (req->content_len <= 0) {
            send_json_error(req, "400 Bad Request", "empty upload");
            return ESP_FAIL;
        }
        if (static_cast<std::size_t>(req->content_len) > _max_upload_bytes) {
            send_json_error(req, "413 Payload Too Large", "file too large");
            return ESP_FAIL;
        }

        const std::string batch_token = query_value(req, "batch");
        const bool reset =
            query_flag_set(req, "reset") || batch_token.empty() || batch_token != self->_active_batch_token;
        const bool last_in_batch = query_flag_set(req, "last") || batch_token.empty();

        if (reset) {
            moments::storage::delete_all_photos();
            self->_batch_id           = GetHAL().millis();
            self->_active_batch_token = batch_token;
            self->_photos_in_batch    = 0;
        }

        if (self->_photos_in_batch >= moments::storage::kMaxPhotos) {
            send_json_error(req, "409 Conflict", "too many photos");
            return ESP_FAIL;
        }

        const long free_bytes = moments::storage::free_space_bytes();
        if (free_bytes >= 0 && free_bytes < req->content_len + 65536) {
            send_json_error(req, "507 Insufficient Storage", "not enough storage");
            return ESP_FAIL;
        }

        const std::string temp_path = moments::storage::temp_upload_path();
        FILE* file                  = fopen(temp_path.c_str(), "wb");
        if (file == nullptr) {
            send_json_error(req, "500 Internal Server Error", "failed to store image");
            return ESP_FAIL;
        }

        char buffer[4096];
        std::size_t remaining = static_cast<std::size_t>(req->content_len);
        bool write_failed     = false;
        while (remaining > 0) {
            const int to_read  = static_cast<int>(remaining < sizeof(buffer) ? remaining : sizeof(buffer));
            const int received = httpd_req_recv(req, buffer, to_read);
            if (received <= 0) {
                write_failed = true;
                break;
            }
            if (fwrite(buffer, 1, static_cast<std::size_t>(received), file) != static_cast<std::size_t>(received)) {
                write_failed = true;
                break;
            }
            remaining -= static_cast<std::size_t>(received);
        }
        fclose(file);

        if (write_failed) {
            unlink(temp_path.c_str());
            send_json_error(req, "500 Internal Server Error", "upload interrupted");
            return ESP_FAIL;
        }

        int width  = 0;
        int height = 0;
        if (!moments::storage::read_bmp_dimensions(temp_path, width, height) ||
            width != moments::storage::kPhotoWidth || height != moments::storage::kPhotoHeight) {
            unlink(temp_path.c_str());
            send_json_error(req, "400 Bad Request", "invalid preview image");
            return ESP_FAIL;
        }

        const std::string final_path = moments::storage::make_photo_path(self->_batch_id, self->_photos_in_batch);
        if (rename(temp_path.c_str(), final_path.c_str()) != 0) {
            unlink(temp_path.c_str());
            send_json_error(req, "500 Internal Server Error", "failed to finalize image");
            return ESP_FAIL;
        }
        self->_photos_in_batch += 1;

        self->log("Uploading " + std::to_string(self->_photos_in_batch) + "/" +
                   std::to_string(moments::storage::kMaxPhotos) + "...");
        if (last_in_batch) {
            self->log("Upload complete, tap Done to finish");
        }

        send_json(req, "{\"ok\":true,\"photos\":" + std::to_string(self->_photos_in_batch) + "}");
        return ESP_OK;
    }

    httpd_handle_t _server          = nullptr;
    EventGroupHandle_t _event_group = nullptr;
    std::function<void(std::string_view)> _on_log;
    std::string _ssid;
    std::unique_ptr<DnsServer> _dns_server;

    std::string _active_batch_token;
    uint32_t _batch_id           = 0;
    std::size_t _photos_in_batch = 0;
};

}  // namespace

void run_upload_mode(const std::function<void(std::string_view)>& onLog)
{
    Session(onLog).run();
}

}  // namespace moments::net
