#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "frozen.h"

#include "auth.h"
#include "main_thread.h"
#include "smartqua_config.h"
#include "drivers/system_info.h"
#include "network/wifi_manager.h"
#include "network/webserver.h"
#include "rest/devices_rest.h"
#include "utils/idf_utils.h"
#include "utils/logger.h"

#include <chrono>
#include <optional>
#include <cstdint>
#include <thread>

std::unique_ptr<GlobalStoreType, SPIRAMDeleter<GlobalStoreType>> global_store;

void init_timezone() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", true);
    tzset();
}

void print_health(void *) {
    auto buffer = LargeBufferPoolType::get_free_buffer();

    std::time_t now;
    std::tm timeinfo;
    std::array<char, 64> timeout;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(timeout.data(), timeout.size(), "%c", &timeinfo);

    auto bytesWritten = CurrentSystem::printSystemHealthToString(buffer->data(), buffer->size());

    Logger::log(LogLevel::Warning, "%s", buffer->data());
    Logger::log(LogLevel::Warning, "%s", timeout.data());
}

void *mainTask(void *pvParameters) {
    // TODO: This first log should consider the device we are running this on (don't use esp-idf here)
    ESP_LOGI(__PRETTY_FUNCTION__, "Restarting ...");

     i2cdev_init();

    Logger::log(LogLevel::Info, "Installed general logger");

    wifi_config<WIFI_MODE_STA> config{
            .creds{
                .ssid{DEFAULT_WIFI_SSID}, 
                .password{DEFAULT_WIFI_PASS}
            },
            .reconnect_tries = WifiReconnectPolicy::infinite,
            .retry_time = std::chrono::milliseconds(1000)
    };

    esp_event_loop_create_default();

    WifiManager<WIFI_MODE_STA> wifi(config);
    // TODO: if unit has rtc, continue and cre
    wifi.awaitConnection(std::chrono::seconds(5));

    init_timezone();
    wait_for_clock_sync();

    // Logger::ignoreLogsBelow(LogLevel::Warning);

    Logger::log(LogLevel::Info, "Init global store now");

    void *globalStoreMemory = heap_caps_malloc(sizeof(GlobalStoreType), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    global_store = std::unique_ptr<GlobalStoreType, SPIRAMDeleter<GlobalStoreType>>(
        new (globalStoreMemory) GlobalStoreType, SPIRAMDeleter<GlobalStoreType>{});
    global_store->initValues();

    // Unsecure server only has access to this specific folder, which hosts the webapp, for this results in issues
    // webserver<security_level::unsecured> app_server("/external/app_data");
    WebServer<WebServerSecurityLevel::unsecured> api_server("");
    api_server.registerHandler("/api/v1/devices",
                               1ULL << HTTP_GET | 1ULL << HTTP_POST | 1ULL << HTTP_PUT | 1ULL << HTTP_DELETE |
                               1ULL << HTTP_PATCH,
                               do_devices);

    sntp_clock clock;

    auto resource = MainTaskPool::postTask(TaskDescription{
            .single_shot = false,
            .func_ptr = print_health,
            .interval = std::chrono::seconds(10),
            .argument = nullptr,
            .description = "Heartbeat Thread"
    });

    MainTaskPool::doWork();

    // Shouldn't be reached
    pthread_exit(nullptr);
}