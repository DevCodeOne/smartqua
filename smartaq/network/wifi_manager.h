#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"

#include "storage/nvs_flash_utils.h"
#include "network/network_info.h"

static constexpr uint8_t WIFI_CONNECTED_BIT = BIT0;
static constexpr uint8_t WIFI_FAIL_BIT = BIT1;

template <wifi_mode_t Mode>
class WifiManager;

struct WifiCredentials {
    std::array<char, 32> ssid;
    std::array<char, 32> password;
};

enum struct WifiReconnectPolicy : uint32_t {
    never = 0,
    once = 1,
    infinite = std::numeric_limits<uint32_t>::max()
};

template <wifi_mode_t Mode>
struct wifi_config;

template <>
struct wifi_config<wifi_mode_t::WIFI_MODE_STA> {
    WifiCredentials creds;
    WifiReconnectPolicy reconnect_tries{WifiReconnectPolicy::infinite};
    std::chrono::milliseconds retry_time{1000};
};

template <>
class WifiManager<wifi_mode_t::WIFI_MODE_STA> {
   public:
    explicit WifiManager(const wifi_config<wifi_mode_t::WIFI_MODE_STA> &config)
        : m_config(config), m_wifi_event_group(xEventGroupCreate()) {
        esp_netif_init();

        esp_netif_create_default_wifi_sta();

        wifi_init_config_t tmp_init_conf = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&tmp_init_conf);

        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                   &WifiManager::eventHandler, this);
        esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                   &WifiManager::eventHandler, this);

        std::memset(m_wifi_conf.sta.ssid, 0, sizeof(m_wifi_conf.sta.ssid));
        std::memset(m_wifi_conf.sta.password, 0, sizeof(m_wifi_conf.sta.password));
        std::memcpy(m_wifi_conf.sta.ssid, m_config.creds.ssid.data(),
                    strlen(m_config.creds.ssid.data()));
        std::memcpy(m_wifi_conf.sta.password, m_config.creds.password.data(),
                    strlen(m_config.creds.password.data()));

        // Logger::log(LogLevel::Info, "SSID: %s PASS: %s", m_wifi_conf.sta.ssid, m_wifi_conf.sta.password);

        m_wifi_conf.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(wifi_interface_t::WIFI_IF_STA, &m_wifi_conf);
        esp_wifi_start();
        esp_wifi_set_ps(wifi_ps_type_t::WIFI_PS_NONE);
    }

    ~WifiManager() {
        esp_event_handler_unregister(WIFI_EVENT, IP_EVENT_STA_GOT_IP,
                                     &WifiManager::eventHandler);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                     &WifiManager::eventHandler);
        vEventGroupDelete(m_wifi_event_group);
    }

    bool awaitConnection(std::chrono::milliseconds waitFor = std::chrono::seconds(5)) {
        EventBits_t bits = xEventGroupWaitBits(
                m_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE,
                pdFALSE, waitFor.count() / portTICK_PERIOD_MS);

        return (bits & WIFI_CONNECTED_BIT) && !(bits & WIFI_FAIL_BIT);
    }

    // TODO: add delay to reconnect
    static void eventHandler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data) {
        auto thiz = reinterpret_cast<WifiManager *>(arg);
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_base == WIFI_EVENT &&
                   event_id == WIFI_EVENT_STA_DISCONNECTED) {
            if (thiz->retry_num <
                static_cast<uint32_t>(thiz->m_config.reconnect_tries)) {
                esp_wifi_connect();
                thiz->retry_num++;
                Logger::log(LogLevel::Warning, "Connection to the AP failed");
            } else {
                xEventGroupSetBits(thiz->m_wifi_event_group, WIFI_FAIL_BIT);
            }
            NetworkInfo::disallowNetwork();
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            Logger::log(LogLevel::Info, "got ip:" IPSTR,
                     IP2STR(&event->ip_info.ip));
            thiz->retry_num = 0;
            xEventGroupSetBits(thiz->m_wifi_event_group, WIFI_CONNECTED_BIT);
            NetworkInfo::allowNetwork();
        }
    }

   private:
    wifi_config<wifi_mode_t::WIFI_MODE_STA> m_config;
    wifi_config_t m_wifi_conf{};
    EventGroupHandle_t m_wifi_event_group;
    nvs_flash m_nvs;
    int32_t retry_num = 0;
};
