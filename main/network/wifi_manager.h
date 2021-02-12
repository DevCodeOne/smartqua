#pragma once

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"

#include "storage/nvs_flash_utils.h"

static constexpr uint8_t wifi_connected_bit = BIT0;
static constexpr uint8_t wifi_fail_bit = BIT1;

template <wifi_mode_t Mode>
class wifi_manager;

struct wifi_credentials {
    std::array<char, 32> ssid;
    std::array<char, 32> password;
};

enum struct wifi_reconnect_policy : uint32_t {
    infinite = std::numeric_limits<uint32_t>::max(),
    never = 0,
    once = 1
};

template <wifi_mode_t Mode>
struct wifi_config;

template <>
struct wifi_config<wifi_mode_t::WIFI_MODE_STA> {
    wifi_credentials creds;
    wifi_reconnect_policy reconnect_tries{wifi_reconnect_policy::infinite};
    std::chrono::milliseconds retry_time{1000};
};

template <>
class wifi_manager<wifi_mode_t::WIFI_MODE_STA> {
   public:
    wifi_manager(const wifi_config<wifi_mode_t::WIFI_MODE_STA> &config)
        : m_config(config), m_wifi_event_group(xEventGroupCreate()) {
        esp_netif_init();

        esp_netif_create_default_wifi_sta();

        wifi_init_config_t tmp_init_conf = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&tmp_init_conf);

        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                   &wifi_manager::event_handler, this);
        esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                   &wifi_manager::event_handler, this);

        std::memset(m_wifi_conf.sta.ssid, 0, sizeof(m_wifi_conf.sta.ssid));
        std::memset(m_wifi_conf.sta.password, 0, sizeof(m_wifi_conf.sta.password));
        std::memcpy(m_wifi_conf.sta.ssid, m_config.creds.ssid.data(),
                    strlen(m_config.creds.ssid.data()));
        std::memcpy(m_wifi_conf.sta.password, m_config.creds.password.data(),
                    strlen(m_config.creds.password.data()));

        ESP_LOGI(__PRETTY_FUNCTION__, "SSID: %s PASS: %s", m_wifi_conf.sta.ssid, m_wifi_conf.sta.password);

        m_wifi_conf.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(wifi_interface_t::WIFI_IF_STA, &m_wifi_conf);
        esp_wifi_start();
    }

    ~wifi_manager() {
        esp_event_handler_unregister(WIFI_EVENT, IP_EVENT_STA_GOT_IP,
                                     &wifi_manager::event_handler);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                     &wifi_manager::event_handler);
        vEventGroupDelete(m_wifi_event_group);
    }

    // TODO: add max wait time
    bool await_connection() {
        EventBits_t bits = xEventGroupWaitBits(
            m_wifi_event_group, wifi_connected_bit | wifi_fail_bit, pdFALSE,
            pdFALSE, portMAX_DELAY);

        return (bits & wifi_connected_bit) && !(bits & wifi_fail_bit);
    }

    operator bool() const;

    // TODO: add delay to reconnect
    static void event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
        auto thiz = reinterpret_cast<wifi_manager *>(arg);
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_base == WIFI_EVENT &&
                   event_id == WIFI_EVENT_STA_DISCONNECTED) {
            if (thiz->retry_num <
                static_cast<uint32_t>(thiz->m_config.reconnect_tries)) {
                esp_wifi_connect();
                thiz->retry_num++;
                ESP_LOGI(__PRETTY_FUNCTION__, "retry to connect to the AP");
            } else {
                xEventGroupSetBits(thiz->m_wifi_event_group, wifi_fail_bit);
            }
            ESP_LOGI(__PRETTY_FUNCTION__, "connect to the AP fail");
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(__PRETTY_FUNCTION__, "got ip:" IPSTR,
                     IP2STR(&event->ip_info.ip));
            thiz->retry_num = 0;
            xEventGroupSetBits(thiz->m_wifi_event_group, wifi_connected_bit);
        }
    }

   private:
    wifi_config<wifi_mode_t::WIFI_MODE_STA> m_config;
    wifi_config_t m_wifi_conf{};
    EventGroupHandle_t m_wifi_event_group;
    nvs_flash m_nvs;
    int32_t retry_num = 0;
};
