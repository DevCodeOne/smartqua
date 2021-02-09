#include <stdio.h>
#include <string.h>

#include <cmath>

#include "auth.h"
#include "data.h"
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
#include "scale.h"
#include "settings.h"
#include "wifi_manager.h"
#include "webserver.h"
#include "scale_rest.h"
#include "pwm_rest.h"
#include "aq_main.h"
// clang-format on

static constexpr uint8_t file_path_max = 32;
static constexpr char log_tag[] = "Co2_Scale";

int retry_num = 0;
// loadcell scale{18, 19};

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

    webserver server;
    server.register_handler({.uri = "/api/v1/tare",
                             .method = HTTP_POST,
                             .handler = tare_scale,
                             .user_ctx = nullptr});
    server.register_handler({.uri = "/api/v1/contained-co2",
                             .method = HTTP_POST,
                             .handler = set_contained_co2,
                             .user_ctx = nullptr});
    server.register_handler({.uri = "/api/v1/load",
                             .method = HTTP_GET,
                             .handler = get_load,
                             .user_ctx = nullptr});

    server.register_handler({.uri ="/api/v1/pwm/*",
                             .method = HTTP_GET,
                             .handler = get_pwm_outputs,
                             .user_ctx = nullptr});

    server.register_handler({.uri ="/api/v1/pwm",
                             .method = HTTP_POST,
                             .handler = post_pwm_output,
                             .user_ctx = nullptr});

    server.register_handler({.uri ="/api/v1/pwm",
                             .method = HTTP_PATCH,
                             .handler = set_pwm_output,
                             .user_ctx = nullptr});
    
    server.register_file_handler();

    while (1) {
        ESP_LOGI(log_tag, "%s", __PRETTY_FUNCTION__);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void mainTask(void *pvParameters) {
    ESP_LOGI(log_tag, "Starting scale");

    /*
    while (1) {
        auto result = scale.init_scale();
        if (result == loadcell_status::failure) {
            ESP_LOGI(log_tag, "Failed to initialize scale");
        } else {
            ESP_LOGI(log_tag, "Initialized scale");
            break;
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }*/

    // Store current tare value
    // auto result = s.tare();
    // ESP_ERROR_CHECK(update_load_data(storage_handle, &data));
    // data.offset = s.get_offset();
    // if (result == loadcell_status::failure) {
    //     printf("Failed to tare scale");
    // }
    // ESP_LOGI(log_tag, "Offset : %d",
    //          data.get_value<scale_setting_indices::offset>());

    scale_event e{};
    global_store.read_event(e);
    // scale.set_offset(e.data.offset);
    // scale.set_scale(201.0f);

    while (1) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        /*auto result = scale.read_value(&value);

        if (result == loadcell_status::failure) {
            ESP_LOGI(log_tag, "Failed to read from scale");
            continue;
        }

        ESP_LOGI(log_tag, "Read value %d.%03dG \t Offset %d",
                 static_cast<int32_t>(value),
                 static_cast<int32_t>(
                     std::fabs(value - static_cast<int32_t>(value)) * 1000), 0);*/
        ESP_LOGI(log_tag, "%s", __PRETTY_FUNCTION__);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

extern "C" {

void app_main() {
    xTaskCreatePinnedToCore(mainTask, "mainTask", 4096, nullptr, 5, nullptr, 0);
    xTaskCreatePinnedToCore(networkTask, "networkTask", 4096 * 4, nullptr, 5, nullptr, 1);
}
}
