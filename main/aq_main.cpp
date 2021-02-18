#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "frozen.h"
#include "nvs.h"
#include "nvs_flash.h"

// clang-format off
#include "auth.h"
#include "network/wifi_manager.h"
#include "network/webserver.h"
#include "rest/scale_rest.h"
#include "rest/devices_rest.h"
#include "rest/soft_timers_rest.h"
#include "utils/idf-utils.h"
#include "aq_main.h"
// clang-format on

static constexpr char log_tag[] = "Aq_main";

decltype(global_store) global_store;

httpd_handle_t http_server_handle = nullptr;

esp_err_t init_filesystem() {
    esp_vfs_spiffs_conf_t config = {};
    config.base_path = "/storage";
    config.partition_label = nullptr;
    /* Maximum number of files which can be opened at the same time */
    config.max_files = 4;
    config.format_if_mount_failed = false;
    return esp_vfs_spiffs_register(&config);
}

void init_timezone() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", true);
    tzset();
}

void networkTask(void *pvParameters) {
    // clang-format off
    wifi_config<WIFI_MODE_STA> config{
            .creds{
                .ssid{WIFI_SSID}, 
                .password{WIFI_PASS}
            },
            .reconnect_tries = wifi_reconnect_policy::infinite,
            .retry_time = std::chrono::milliseconds(1000)
    };
    // clang-format on

    esp_event_loop_create_default();

    wifi_manager<WIFI_MODE_STA> wifi(config);
    wifi.await_connection();

    init_filesystem();
    init_timezone();

    webserver server;
    server.register_handler({.uri ="/api/v1/timers/*",
                             .method = HTTP_GET,
                             .handler = get_timers_rest,
                             .user_ctx = nullptr});

    server.register_handler({.uri ="/api/v1/timers",
                             .method = HTTP_POST,
                             .handler = post_timer_rest,
                             .user_ctx = nullptr});

    server.register_handler({.uri ="/api/v1/timers/*",
                             .method = HTTP_PUT,
                             .handler = post_timer_rest,
                             .user_ctx = nullptr});

    server.register_handler({.uri ="/api/v1/timers/*",
                             .method = HTTP_DELETE,
                             .handler = remove_timer_rest,
                             .user_ctx = nullptr});

    server.register_handler({.uri ="/api/v1/timers/*",
                             .method = HTTP_PATCH,
                             .handler = set_timer_rest,
                             .user_ctx = nullptr});

    server.register_handler({.uri ="/api/v1/devices/*",
                             .method = HTTP_GET,
                             .handler = get_devices,
                             .user_ctx = nullptr});

    server.register_handler({.uri ="/api/v1/devices",
                             .method = HTTP_POST,
                             .handler = post_device,
                             .user_ctx = nullptr});

    server.register_handler({.uri ="/api/v1/devices/*",
                             .method = HTTP_PUT,
                             .handler = post_device,
                             .user_ctx = nullptr});

    server.register_handler({.uri ="/api/v1/devices/*",
                             .method = HTTP_DELETE,
                             .handler = remove_device,
                             .user_ctx = nullptr});

    server.register_handler({.uri ="/api/v1/devices/*",
                             .method = HTTP_PATCH,
                             .handler = set_device,
                             .user_ctx = nullptr});

    
    server.register_file_handler();

    sntp_clock clock;
    time_t now;
    tm timeinfo;
    std::array<char, 64> timeout;

    while (1) {
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(timeout.data(), timeout.size(), "%c", &timeinfo);

        ESP_LOGI(log_tag, "Main Loop current time %s", timeout.data());

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

extern "C" {

void app_main() {
    xTaskCreatePinnedToCore(networkTask, "networkTask", 4096 * 8, nullptr, 5, nullptr, 1);
}
}
