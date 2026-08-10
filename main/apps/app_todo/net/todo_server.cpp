/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "todo_server.h"

#include "../storage/todo_storage.h"

#include <cstring>
#include <memory>
#include <mutex>
#include <string>

#include <ArduinoJson.h>
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

namespace todo::net {
namespace {

constexpr const char* _tag                = "TodoManage";
constexpr const char* _ap_ssid_prefix     = "M5StopWatch";
constexpr const char* _ap_ssid_suffix     = "-Todo";
constexpr const char* _ap_url             = "http://192.168.4.1";
constexpr EventBits_t _exit_requested_bit = BIT0;
constexpr std::size_t _max_body_bytes     = 1024;

extern const char todo_manage_html_start[] asm("_binary_todo_manage_html_start");
extern const char todo_manage_html_end[] asm("_binary_todo_manage_html_end");

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

std::string items_to_json()
{
    JsonDocument doc;
    JsonArray items_array = doc["items"].to<JsonArray>();
    for (const auto& item : todo::storage::load_items()) {
        JsonObject item_obj   = items_array.add<JsonObject>();
        item_obj["id"]        = item.id;
        item_obj["text"]      = item.text;
        item_obj["completed"] = item.completed;
    }

    std::string output;
    serializeJson(doc, output);
    return output;
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

        log("Todo list updated");
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
        config.stack_size        = 8192;
        config.uri_match_fn      = httpd_uri_match_wildcard;

        esp_err_t ret = httpd_start(&_server, &config);
        if (ret != ESP_OK) {
            log("Failed to start web server");
            return false;
        }

        httpd_uri_t index = {.uri = "/", .method = HTTP_GET, .handler = &Session::handle_index, .user_ctx = this};
        httpd_uri_t items_get = {
            .uri = "/items", .method = HTTP_GET, .handler = &Session::handle_items_get, .user_ctx = this};
        httpd_uri_t items_add = {
            .uri = "/items", .method = HTTP_POST, .handler = &Session::handle_items_add, .user_ctx = this};
        httpd_uri_t items_edit = {
            .uri = "/items/edit", .method = HTTP_POST, .handler = &Session::handle_items_edit, .user_ctx = this};
        httpd_uri_t items_delete = {
            .uri = "/items/delete", .method = HTTP_POST, .handler = &Session::handle_items_delete, .user_ctx = this};
        httpd_uri_t close = {
            .uri = "/close", .method = HTTP_POST, .handler = &Session::handle_close, .user_ctx = this};
        httpd_uri_t captive = {
            .uri = nullptr, .method = HTTP_GET, .handler = &Session::handle_captive_portal, .user_ctx = this};

        esp_err_t reg = httpd_register_uri_handler(_server, &index);
        if (reg == ESP_OK) {
            reg = httpd_register_uri_handler(_server, &items_get);
        }
        if (reg == ESP_OK) {
            reg = httpd_register_uri_handler(_server, &items_add);
        }
        if (reg == ESP_OK) {
            reg = httpd_register_uri_handler(_server, &items_edit);
        }
        if (reg == ESP_OK) {
            reg = httpd_register_uri_handler(_server, &items_delete);
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

    // Reads the whole request body (bounded to _max_body_bytes) into a
    // std::string. Returns false (and already responds with an error) if
    // the body is missing, too large, or a socket error occurs.
    static bool read_body(httpd_req_t* req, std::string& out)
    {
        if (req->content_len <= 0) {
            send_json_error(req, "400 Bad Request", "empty body");
            return false;
        }
        if (static_cast<std::size_t>(req->content_len) > _max_body_bytes) {
            send_json_error(req, "413 Payload Too Large", "body too large");
            return false;
        }

        out.resize(static_cast<std::size_t>(req->content_len));
        std::size_t offset = 0;
        while (offset < out.size()) {
            const int received = httpd_req_recv(req, out.data() + offset, out.size() - offset);
            if (received <= 0) {
                send_json_error(req, "400 Bad Request", "failed to read body");
                return false;
            }
            offset += static_cast<std::size_t>(received);
        }

        return true;
    }

    static esp_err_t handle_index(httpd_req_t* req)
    {
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        httpd_resp_send(req, todo_manage_html_start, todo_manage_html_end - todo_manage_html_start);
        return ESP_OK;
    }

    static esp_err_t handle_items_get(httpd_req_t* req)
    {
        send_json(req, items_to_json());
        return ESP_OK;
    }

    static esp_err_t handle_items_add(httpd_req_t* req)
    {
        auto* self = self_from_request(req);
        std::string body;
        if (!read_body(req, body)) {
            return ESP_FAIL;
        }

        JsonDocument doc;
        if (deserializeJson(doc, body) != DeserializationError::Ok) {
            send_json_error(req, "400 Bad Request", "invalid json");
            return ESP_FAIL;
        }

        const std::string text = doc["text"] | "";
        const uint32_t id      = todo::storage::add_item(text);
        if (id == 0) {
            send_json_error(req, "400 Bad Request", "invalid text or list is full");
            return ESP_FAIL;
        }

        if (self != nullptr) {
            self->log("Added: " + text);
        }
        send_json(req, "{\"ok\":true,\"id\":" + std::to_string(id) + "}");
        return ESP_OK;
    }

    static esp_err_t handle_items_edit(httpd_req_t* req)
    {
        std::string body;
        if (!read_body(req, body)) {
            return ESP_FAIL;
        }

        JsonDocument doc;
        if (deserializeJson(doc, body) != DeserializationError::Ok) {
            send_json_error(req, "400 Bad Request", "invalid json");
            return ESP_FAIL;
        }

        const uint32_t id      = doc["id"] | 0;
        const std::string text = doc["text"] | "";
        if (!todo::storage::edit_item(id, text)) {
            send_json_error(req, "404 Not Found", "item not found or invalid text");
            return ESP_FAIL;
        }

        send_json(req, "{\"ok\":true}");
        return ESP_OK;
    }

    static esp_err_t handle_items_delete(httpd_req_t* req)
    {
        std::string body;
        if (!read_body(req, body)) {
            return ESP_FAIL;
        }

        JsonDocument doc;
        if (deserializeJson(doc, body) != DeserializationError::Ok) {
            send_json_error(req, "400 Bad Request", "invalid json");
            return ESP_FAIL;
        }

        const uint32_t id = doc["id"] | 0;
        if (!todo::storage::delete_item(id)) {
            send_json_error(req, "404 Not Found", "item not found");
            return ESP_FAIL;
        }

        send_json(req, "{\"ok\":true}");
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

    httpd_handle_t _server          = nullptr;
    EventGroupHandle_t _event_group = nullptr;
    std::function<void(std::string_view)> _on_log;
    std::string _ssid;
    std::unique_ptr<DnsServer> _dns_server;
};

}  // namespace

void run_management_mode(const std::function<void(std::string_view)>& onLog)
{
    Session(onLog).run();
}

}  // namespace todo::net
