#include <cstdio>

#include "aq_main.h"
#include "pwm_rest.h"
#include "json_utils.h"
#include "http_utils.h"

esp_err_t get_pwm_outputs(httpd_req *req) {
    std::string_view uri_view = req->uri;
    auto argument = uri_view.find_last_of("/");
    std::optional<int> index{};

    if (argument + 1 != uri_view.size()) {
        int new_index = 0;
        int result = std::sscanf(uri_view.data() + argument + 1, "%d", &new_index);

        if (result) {
            index = new_index;
        }
    } 

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    std::array<char, 2048> buf;
    json_out answer = JSON_OUT_BUF(buf.data(), buf.size());
    int answer_len = 0;

    if (index) {
        retrieve_single_pwm single_pwm{ .index = static_cast<size_t>(*index) };
        global_store.read_event(single_pwm);

        if (single_pwm.data) {
            answer_len = json_printf(&answer, "%M", json_printf_single<std::decay_t<decltype(*single_pwm.data)>>, &(*single_pwm.data));
        }
    } else {
        retrieve_overview_pwm<pwm_settings::num_outputs> pwm_overiew{.full = false};
        global_store.read_event(pwm_overiew);
        answer_len = json_printf(&answer, "{ %Q : %M }", "pwm_outputs", json_printf_single<decltype(pwm_overiew.data)>, &pwm_overiew.data);
    }

    if (answer_len) {
        send_in_chunks(req, buf, answer_len);
    } else {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Couldn't find this output");
    }

    return ESP_OK;
}

esp_err_t post_pwm_output(httpd_req *req) {
    return ESP_OK;
}

esp_err_t set_pwm_output(httpd_req *req) {
    return ESP_OK;
}