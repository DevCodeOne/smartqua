#include "esp_http_server.h"
#include "spi_flash_mmap.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portable.h"
#include "freertos/task.h"
#include "frozen.h"
#include "nvs.h"
#include "nvs_flash.h"


// clang-format off
// #include "utils/sd_filesystem.h"
#include "auth.h"
#include "network/wifi_manager.h"
#include "network/webserver.h"
#include "rest/stats_rest.h"
#include "rest/devices_rest.h"
#include "smartqua_config.h"
#include "utils/idf_utils.h"
#include "utils/logger.h"
#include "aq_main.h"

#include <chrono>
#include <cstdint>
#include <pthread.h>
#include <thread>
// clang-format on

std::atomic_bool canUseNetwork{false};

std::optional<GlobalStoreType> global_store;

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
    i2cdev_init();

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
    global_store = std::make_optional<GlobalStoreType>();
    global_store->initValues();

    // Unsecure server only has access to this specific folder, which hosts the webapp, for this results in issues
    // webserver<security_level::unsecured> app_server("/external/app_data");
    WebServer<WebServerSecurityLevel::unsecured> api_server("");
    api_server.registerHandler("/api/v1/devices",
                            1ULL << HTTP_GET | 1ULL << HTTP_POST | 1ULL << HTTP_PUT | 1ULL << HTTP_DELETE | 1ULL << HTTP_PATCH,
                            do_devices);

    api_server.registerHandler("/api/v1/stats",
                            1ULL << HTTP_GET | 1ULL << HTTP_POST | 1ULL << HTTP_PUT | 1ULL << HTTP_DELETE | 1ULL << HTTP_PATCH,
                            do_devices);

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

    //pthread_attr_setstack(&attributes, pthreadStack.get(), stackSize);
    // Only stack size can be set on esp-idf
    pthread_attr_setstacksize(&attributes, stack_size);
    if (auto result = pthread_create(&mainThreadHandle, &attributes, networkTask, nullptr); result != 0) {
        ESP_LOGE("Main", "Couldn start main thread");
    }

    if (auto result = pthread_join(mainThreadHandle, nullptr); result != 0) {
        ESP_LOGE("Main", "main thread exited shouldn't happen");
    }

    pthread_attr_destroy(&attributes);
}
}
