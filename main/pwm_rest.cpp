#include "aq_main.h"
#include "pwm_rest.h"
#include "json_utils.h"
#include "http_utils.h"

esp_err_t get_pwm_outputs(httpd_req *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    retrieve_overview_pwm<pwm_settings::num_outputs> pwm_overiew{.full = false};
    global_store.read_event(pwm_overiew);

    std::array<char, 2048> buf;
    json_out answer = JSON_OUT_BUF(buf.data(), buf.size());
    int len = json_printf(&answer, "{ %Q : %M }", "pwm_outputs", json_printf_single<decltype(pwm_overiew.data)>, &pwm_overiew.data);

    send_in_chunks(req, buf, len);
    return ESP_OK;
}

esp_err_t post_pwm_output(httpd_req *req) {
    return ESP_OK;
}

esp_err_t set_pwm_output(httpd_req *req) {
    return ESP_OK;
}