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
// clang-format on

static constexpr uint8_t file_path_max = 32;
static constexpr char log_tag[] = "Co2_Scale";

int retry_num = 0;
loadcell<18, 19> scale;

httpd_handle_t http_server_handle = nullptr;

enum struct scale_setting_indices { offset, weight, scale, contained_co2 };

struct scale_settings {
    using index_type = scale_setting_indices;

    static inline constexpr char name[] = "load_data";

    template <scale_setting_indices T>
    constexpr auto &value() {
        if constexpr (T == scale_setting_indices::offset) {
            return offset;
        } else if constexpr (T == scale_setting_indices::contained_co2) {
            return contained_co2;
        } else {
            return scale;
        }
    }

    int32_t offset = 0;
    float scale = 1.0f;
    int32_t contained_co2;
};

nvs_setting<scale_settings, nvs_init::lazy_load> data;

esp_err_t get_load(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char buf[256];
    json_out answer = JSON_OUT_BUF(buf, sizeof(buf));

    auto off = scale.get_offset();
    auto sc = scale.get_scale();
    float current_weight = 0;
    auto result = scale.read_scale(&current_weight);

    if (result != loadcell_status::success) {
        json_printf(&answer, "{ %Q : %Q }", "info", "Scale error");
        httpd_resp_sendstr(req, buf);
        return ESP_OK;
    }

    json_printf(&answer, "{ %Q : %Q, %Q : %d, %Q : %d, %Q : %d}", "info", "OK",
                "tare", static_cast<int32_t>(off / sc), "load",
                static_cast<int32_t>(current_weight), "contained_co2", 0);
    ESP_LOGE(log_tag, "%s", buf);

    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

esp_err_t set_contained_co2(httpd_req *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char buf[512];
    json_out answer = JSON_OUT_BUF(buf, sizeof(buf));

    size_t recv_size =
        req->content_len < sizeof(buf) ? req->content_len : sizeof(buf);

    if (recv_size == 0) {
        json_printf(&answer, "{ %Q : %s}", "info", "No data");
        httpd_resp_sendstr(req, buf);
        return ESP_OK;
    }

    if (httpd_req_recv(req, buf, recv_size) <= 0) {
        return ESP_FAIL;
    }

    int32_t contained_co2 = 0;

    json_scanf(buf, recv_size, "{ contained_co2 : %d }", &contained_co2);
    ESP_LOGI(log_tag, "%s %d", buf, contained_co2);

    data.set_value<scale_setting_indices::contained_co2>(contained_co2);

    json_printf(&answer, "{ %Q : %Q}", "info", "OK");
    httpd_resp_sendstr(req, buf);

    return ESP_OK;
}

esp_err_t tare_scale(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char buf[256];

    json_out answer = JSON_OUT_BUF(buf, sizeof(buf));

    scale.tare();
    data.set_value<scale_setting_indices::offset>(scale.get_offset());

    json_printf(&answer, "{ %Q : %Q}", "info", "OK");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

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

    wifi_manager<WIFI_MODE_STA> wifi(config);
    wifi.await_connection();

    init_filesystem();

    webserver server;
    server.register_handler({.uri = "/api/v1/load",
                             .method = HTTP_GET,
                             .handler = get_load,
                             .user_ctx = nullptr});
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

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void mainTask(void *pvParameters) {
    ESP_LOGI(log_tag, "Starting scale");

    float value = 0;
    while (1) {
        auto result = scale.init_scale();
        if (result == loadcell_status::failure) {
            ESP_LOGI(log_tag, "Failed to initialize scale");
        } else {
            ESP_LOGI(log_tag, "Initialized scale");
            break;
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    // Store current tare value
    // auto result = s.tare();
    // ESP_ERROR_CHECK(update_load_data(storage_handle, &data));
    // data.offset = s.get_offset();
    // if (result == loadcell_status::failure) {
    //     printf("Failed to tare scale");
    // }
    // ESP_LOGI(log_tag, "Offset : %d",
    //          data.get_value<scale_setting_indices::offset>());

    scale.set_offset(data.get_value<scale_setting_indices::offset>());
    scale.set_scale(201.0f);

    while (1) {
        auto result = scale.read_in_units(10, &value);

        vTaskDelay(500 / portTICK_PERIOD_MS);

        if (result.second == loadcell_status::failure) {
            ESP_LOGI(log_tag, "Failed to read from scale");
            continue;
        }

        ESP_LOGI(log_tag, "Read value %d.%03dG \t Offset %d",
                 static_cast<int32_t>(value),
                 static_cast<int32_t>(
                     std::fabs(value - static_cast<int32_t>(value)) * 1000),
                 data.get_value<scale_setting_indices::offset>());
    }
}

extern "C" {

void app_main() {
    // Nvs has to be initialized for the wifi
    data.initialize();

    xTaskCreatePinnedToCore(mainTask, "mainTask", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(networkTask, "networkTask", 4096 * 4, NULL, 5, NULL,
                            1);
}
}
