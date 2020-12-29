#include <stdio.h>
#include <string.h>

#include <cmath>

#include "auth.h"
#include "data.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "frozen.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "scale.h"
#include "settings.h"

static constexpr uint8_t file_path_max = 32;
static constexpr char log_tag[] = "Co2_Scale";
static loadcell<18, 19> scale;
static int retry_num = 0;
char wifi_ssid[] = WIFI_SSID;
char wifi_passphrase[] = WIFI_PASS;

httpd_handle_t http_server_handle = nullptr;

static constexpr uint8_t wifi_connected_bit = BIT0;
static constexpr uint8_t wifi_fail_bit = BIT1;
static EventGroupHandle_t wifi_event_group;

enum struct scale_setting_indices { offset, weight, scale, contained_co2 };

struct scale_settings {
    using index_type = scale_setting_indices;

    static inline constexpr char name[] = "load_data";

    template <scale_setting_indices T>
    void set_value(auto new_value) {
        if constexpr (T == scale_setting_indices::offset) {
            offset = new_value;
        } else if constexpr (T == scale_setting_indices::contained_co2) {
            contained_co2 = new_value;
        } else if constexpr (T == scale_setting_indices::scale) {
            scale = new_value;
        }
    };

    template <scale_setting_indices T>
    constexpr auto &get_value() {
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

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < MAX_RETRIES) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(log_tag, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifi_event_group, wifi_fail_bit);
        }
        ESP_LOGI(log_tag, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(log_tag, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
        xEventGroupSetBits(wifi_event_group, wifi_connected_bit);
    }
}

esp_err_t get_load(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char buf[256];
    json_out answer = JSON_OUT_BUF(buf, sizeof(buf));
    scale_settings data;

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

// TODO: reimplement
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

bool check_file_extension(const char *filepath, const char *extension) {
    const auto filepath_len = strlen(filepath);
    const auto extension_len = strlen(extension);

    if (filepath_len < extension_len) {
        return false;
    }

    return strcasecmp(filepath + filepath_len - extension_len, extension) == 0;
}

static esp_err_t set_content_type_from_file(httpd_req_t *req,
                                            const char *filepath) {
    const char *type = "text/plain";
    if (check_file_extension(filepath, ".html")) {
        type = "text/html";
    } else if (check_file_extension(filepath, ".js")) {
        type = "application/javascript";
    } else if (check_file_extension(filepath, ".css")) {
        type = "text/css";
    } else if (check_file_extension(filepath, ".png")) {
        type = "image/png";
    } else if (check_file_extension(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (check_file_extension(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

esp_err_t get_file(httpd_req_t *req) {
    char filepath[file_path_max]{"/storage"};
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (req->uri[strlen(req->uri) - 1] == '/') {
        strncat(filepath, "/index.html", sizeof(filepath) - 1);
    } else {
        strncat(filepath, req->uri, sizeof(filepath) - 1);
    }

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(log_tag, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char chunk[512];
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, sizeof(chunk));
        if (read_bytes == -1) {
            ESP_LOGE(log_tag, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(log_tag, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                    "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(log_tag, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, nullptr, 0);
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

esp_err_t init_http_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_ERROR_CHECK(httpd_start(&http_server_handle, &config));

    httpd_uri_t load_handler = {.uri = "/api/v1/load",
                                .method = HTTP_GET,
                                .handler = get_load,
                                .user_ctx = nullptr};
    httpd_register_uri_handler(http_server_handle, &load_handler);

    httpd_uri_t tare_handler = {.uri = "/api/v1/tare",
                                .method = HTTP_POST,
                                .handler = tare_scale,
                                .user_ctx = nullptr};
    httpd_register_uri_handler(http_server_handle, &tare_handler);

    httpd_uri_t contained_co2_handler = {.uri = "/api/v1/contained-co2",
                                         .method = HTTP_POST,
                                         .handler = set_contained_co2,
                                         .user_ctx = nullptr};
    httpd_register_uri_handler(http_server_handle, &contained_co2_handler);

    httpd_uri_t file_handler = {.uri = "/*",
                                .method = HTTP_GET,
                                .handler = get_file,
                                .user_ctx = nullptr};

    httpd_register_uri_handler(http_server_handle, &file_handler);

    return ESP_OK;
}

void networkTask(void *pvParameters) {
    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &event_handler, NULL));

    wifi_config_t wifi_config;
    memcpy(wifi_config.sta.ssid, wifi_ssid, strlen(wifi_ssid));
    memcpy(wifi_config.sta.password, wifi_passphrase, strlen(wifi_passphrase));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    // Doesn't work with these options enabled
    // wifi_config.sta.pmf_cfg.capable = true;
    // wifi_config.sta.pmf_cfg.required = true;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(log_tag, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT)
     * or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler()
     * (see above) */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           wifi_connected_bit | wifi_fail_bit,
                                           pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we
     * can test which event actually happened. */
    if (bits & wifi_connected_bit) {
        ESP_LOGI(log_tag, "connected to ap SSID:%s", wifi_ssid);
    } else if (bits & wifi_fail_bit) {
        ESP_LOGI(log_tag, "Failed to connect to SSID:%s", wifi_ssid);
    } else {
        ESP_LOGE(log_tag, "UNEXPECTED EVENT");
    }

    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                 &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                 &event_handler));

    init_filesystem();
    init_http_server();
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vEventGroupDelete(wifi_event_group);
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

    // Store curren tare value
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
    // Andere Zelle: s.set_scale(570.f);

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
    xTaskCreatePinnedToCore(mainTask, "mainTask", 4096, NULL, 5, NULL, 0);
    // xTaskCreatePinnedToCore(networkTask, "networkTask", 4096 * 4, NULL, 5,
    // NULL,
    //                         1);
}
}
