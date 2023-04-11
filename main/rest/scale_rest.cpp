#include <cstring>

#include "utils/logger.h"
#include "scale_rest.h"
#include "smartqua_config.h"
#include "aq_main.h"

/*
// extern loadcell scale;
static constexpr char log_tag[] = "RestScale";

esp_err_t get_load(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char buf[256];
    json_out answer = JSON_OUT_BUF(buf, sizeof(buf));

    // auto off = scale.get_offset();
    auto off = 0;
    auto sc = 200.0f;
    float current_weight = 0;
    // auto result = scale.read_value(&current_weight);
    auto result = loadcell_status::success;

    if (result != loadcell_status::success) {
        json_printf(&answer, "{ %Q : %Q }", "info", "Scale error");
        httpd_resp_sendstr(req, buf);
        return ESP_OK;
    }

    scale_event se{};
    global_store->read_event(se);

    json_printf(&answer, "{ %Q : %Q, %Q : %d, %Q : %d, %Q : %d}", "info", "OK",
                "tare", static_cast<int32_t>(off / sc), "load",
                static_cast<int32_t>(current_weight), "contained_co2", se.data.contained_co2);
    Logger::log(LogLevel::Error, "%s", buf);

    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

esp_err_t set_contained_co2(httpd_req *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char buf[512];
    std::memset(buf, 0, sizeof(buf));
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
    Logger::log(LogLevel::Info, "%s %d", buf, contained_co2);

    scale_event se{ .type = scale_setting_indices::contained_co2, .data = { .contained_co2 = contained_co2 }};
    global_store->write_event(se);

    json_printf(&answer, "{ %Q : %Q}", "info", "OK");
    httpd_resp_sendstr(req, buf);

    return ESP_OK;
}

esp_err_t tare_scale(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char buf[256];

    json_out answer = JSON_OUT_BUF(buf, sizeof(buf));

    // scale.tare();
    int32_t scale_offset = 0;
    scale_event se{ .type = scale_setting_indices::offset, .data = { .offset = scale_offset }};
    global_store->write_event(se);

    json_printf(&answer, "{ %Q : %Q}", "info", "OK");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}
*/