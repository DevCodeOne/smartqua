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
#include "rest/stats_rest.h"
#include "rest/devices_rest.h"
#include "rest/soft_timers_rest.h"
#include "smartqua_config.h"
#include "utils/idf_utils.h"
#include "utils/sd_filesystem.h"
#include "utils/sd_card_logger.h"
#include "aq_main.h"

#include <chrono>
#include <thread>
// clang-format on

static constexpr char log_tag[] = "Aq_main";

decltype(global_store) global_store;

void init_timezone() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", true);
    tzset();
}

void print_time(void *) {
    std::time_t now;
    std::tm timeinfo;
    std::array<char, 64> timeout;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(timeout.data(), timeout.size(), "%c", &timeinfo);

    ESP_LOGI(log_tag, "Main Loop current time %s", timeout.data());
}

void networkTask(void *pvParameters) {

    sd_card_logger logger;
    logger.install_sd_card_logger();

    wifi_config<WIFI_MODE_STA> config{
            .creds{
                .ssid{WIFI_SSID}, 
                .password{WIFI_PASS}
            },
            .reconnect_tries = wifi_reconnect_policy::infinite,
            .retry_time = std::chrono::milliseconds(1000)
    };

    esp_event_loop_create_default();

    wifi_manager<WIFI_MODE_STA> wifi(config);
    wifi.await_connection();

    init_timezone();
    wait_for_clock_sync();

    // esp_log_level_set("*", ESP_LOG_WARN); 

    global_store.initValues();

    // Unsecure server only has access to this specific folder, which hosts the webapp, for this results in issues
    // webserver<security_level::unsecured> app_server("/external/app_data");
    WebServer<WebServerSecurityLevel::unsecured> api_server("");
    api_server.registerHandler({.uri ="/api/v1/timers*",
                             .method = HTTP_GET,
                             .handler = do_timers,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/timers*",
                             .method = HTTP_POST,
                             .handler = do_timers,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/timers*",
                             .method = HTTP_PUT,
                             .handler = do_timers,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/timers*",
                             .method = HTTP_DELETE,
                             .handler = do_timers,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/timers*",
                             .method = HTTP_PATCH,
                             .handler = do_timers,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/devices*",
                             .method = HTTP_GET,
                             .handler = do_devices,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/devices*",
                             .method = HTTP_POST,
                             .handler = do_devices,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/devices*",
                             .method = HTTP_PUT,
                             .handler = do_devices,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/devices*",
                             .method = HTTP_DELETE,
                             .handler = do_devices,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/devices*",
                             .method = HTTP_PATCH,
                             .handler = do_devices,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/stats*",
                             .method = HTTP_GET,
                             .handler = do_stats,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/stats*",
                             .method = HTTP_POST,
                             .handler = do_stats,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/stats*",
                             .method = HTTP_PUT,
                             .handler = do_stats,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/stats*",
                             .method = HTTP_DELETE,
                             .handler = do_stats,
                             .user_ctx = nullptr});

    api_server.registerHandler({.uri ="/api/v1/stats*",
                             .method = HTTP_PATCH,
                             .handler = do_stats,
                             .user_ctx = nullptr});
    
    sntp_clock clock;

    auto resource = task_pool<max_task_pool_size>::post_task(single_task{
        .single_shot = false,
        .func_ptr = print_time,
        .argument = nullptr
    });

    task_pool<max_task_pool_size>::do_work();
}

extern "C" {

void app_main() {
    xTaskCreatePinnedToCore(networkTask, "networkTask", 4096 * 12, nullptr, 5, nullptr, 1);
}
}
