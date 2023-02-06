#include "esp_http_server.h"
#include "esp_spi_flash.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portable.h"
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
#include "smartqua_config.h"
#include "utils/idf_utils.h"
#include "utils/sd_filesystem.h"
#include "utils/logger.h"
#include "aq_main.h"

#include <chrono>
#include <cstdint>
#include <pthread.h>
#include <thread>
// clang-format on

static constexpr char log_tag[] = "Aq_main";
std::atomic_bool canUseNetwork{false};

decltype(global_store) global_store;

void init_timezone() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", true);
    tzset();
}

void print_health(void *) {
    static auto earlierHeapUsage = decltype(xPortGetFreeHeapSize()){};

    std::time_t now;
    std::tm timeinfo;
    std::array<char, 64> timeout;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(timeout.data(), timeout.size(), "%c", &timeinfo);

    const auto currentFreeHeap = xPortGetFreeHeapSize();
    Logger::log(LogLevel::Warning, "Current free heap space is %d, free heap down by %d bytes, time is %s", 
        currentFreeHeap,
        currentFreeHeap - earlierHeapUsage,
        timeout.data());
    earlierHeapUsage = currentFreeHeap;
}

void *networkTask(void *pvParameters) {
    // TODO: This first log should consider the device we are running this on (don't use esp-idf here)
    ESP_LOGI(__PRETTY_FUNCTION__, "Restarting ...");

    Logger::log(LogLevel::Info, "Installed general logger");

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

    // Logger::ignoreLogsBelow(LogLevel::Warning);

    Logger::log(LogLevel::Info, "Init global store now");
    global_store.initValues();

    // Unsecure server only has access to this specific folder, which hosts the webapp, for this results in issues
    // webserver<security_level::unsecured> app_server("/external/app_data");
    WebServer<WebServerSecurityLevel::unsecured> api_server("");
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
        .func_ptr = print_health,
        .argument = nullptr,
        .description = "Heartbeat Thread"
    });

    task_pool<max_task_pool_size>::do_work();

    return nullptr;
}

extern "C" {

void app_main() {
    pthread_attr_t attributes;
    pthread_t mainThreadHandle;

    pthread_attr_init(&attributes);

    constexpr size_t stackSize = 4096 * 4;

    //pthread_attr_setstack(&attributes, pthreadStack.get(), stackSize);
    // Only stack size can be set on esp-idf
    pthread_attr_setstacksize(&attributes, stackSize);
    if (auto result = pthread_create(&mainThreadHandle, &attributes, networkTask, nullptr); result != 0) {
        ESP_LOGE("Main", "Couldn start main thread");
    }

    if (auto result = pthread_join(mainThreadHandle, nullptr); result != 0) {
        ESP_LOGE("Main", "main thread exited shouldn't happen");
    }

    pthread_attr_destroy(&attributes);
}
}
