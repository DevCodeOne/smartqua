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
#include "rest/settings_rest.h"
#include "utils/logger.h"
#include "utils/esp/idf_utils.h"

#include <chrono>
#include <optional>
#include <cstdint>
#include <thread>

#include "utils/bit_utils.h"
#include "utils/memory_helper.h"

std::unique_ptr<GlobalStoreType, SPIRAMDeleter<GlobalStoreType> > globalStore;

void init_timezone() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", true);
    tzset();
}

void print_health(void *) {
    auto buffer = LargeBufferPoolType::get_free_buffer();

    std::time_t now;
    std::tm timeinfo{};
    std::array<char, 64> timeout{};
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(timeout.data(), timeout.size(), "%c", &timeinfo);

    [[maybe_unused]] const auto bytesWritten = CurrentSystem::printSystemHealthToString(buffer->data(), buffer->size());

    Logger::log(LogLevel::Warning, "%s", buffer->data());
    Logger::log(LogLevel::Warning, "%s", timeout.data());
}

void *doWork(void *)
{
    auto resource = MainTaskPool::postTask(TaskDescription{
         .single_shot = false,
         .func_ptr = print_health,
         .interval = std::chrono::seconds(10),
         .argument = nullptr,
         .description = "Heartbeat Thread"
     });

    MainTaskPool::doWork();
}

void *doWorkThread(void *arg)
{
    doWork(arg);

    pthread_exit(nullptr);
}


std::optional<pthread_t> addThread(pthread_attr_t& attributes)
{
    pthread_t handle;

    //pthread_attr_setstack(&attributes, pthreadStack.get(), stackSize);
    // Only stack size can be set on esp-idf
    int result = pthread_create(&handle, &attributes, doWorkThread, nullptr);
    if (result != 0) {
        ESP_LOGE("Main", "Couldn't start main thread");
        return {};
    }

    return {handle};
}

void *mainTask(void *) {
    // TODO: This first log should consider the device we are running this on (don't use esp-idf here)
    ESP_LOGI(__PRETTY_FUNCTION__, "Restarting ...");

    Logger::log(LogLevel::Info, "Installed general logger");
    i2cdev_init();

    WifiConfig<WIFI_MODE_STA> config{
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
    waitForClockSync();

    // Logger::ignoreLogsBelow(LogLevel::Warning);

    Logger::log(LogLevel::Info, "Init global store now");

    void *globalStoreMemory = heap_caps_malloc(sizeof(GlobalStoreType), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    globalStore = makePointerAt<std::unique_ptr, GlobalStoreType>(globalStoreMemory, SPIRAMDeleter<GlobalStoreType>{});
    globalStore->initValues();

    // Unsecure server only has access to this specific folder, which hosts the webapp, for this results in issues
    // webserver<security_level::unsecured> app_server("/external/app_data");
    WebServer<WebServerSecurityLevel::unsecured> api_server("");
    api_server.registerHandler("/api/v1/devices",
                               CombinedFlagsAtPos<uint32_t,
                                   HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_DELETE, HTTP_PATCH>,
                               do_devices);
    api_server.registerHandler("api/v1/settings", CombinedFlagsAtPos<uint32_t, HTTP_GET, HTTP_POST, HTTP_PUT>,
                               do_settings);

    SntpClock clock;

    static unsigned int stack_size = 7 * 4096 + 2048;

    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, stack_size);

    constexpr size_t additionalNumThreads = 1;

    std::array<std::optional<pthread_t>, additionalNumThreads> handles;

    for (auto &currentHandle : handles)
    {
        currentHandle = addThread(attributes);
    }

    doWork(nullptr);

    for (auto &currentHandle : handles)
    {
        if (currentHandle && pthread_join(*currentHandle, nullptr) != 0)
        {
            ESP_LOGE("Main", "Couldn't join thread");
        }

    }

    ESP_LOGE("Main", "Main thread exited shouldn't happen");
    // pthread_attr_destroy(&attributes);
    pthread_exit(nullptr);
}
