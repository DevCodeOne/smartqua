#include "soft_timers_rest.h"

#include <array>
#include <cstdint>

#include "frozen.h"

#include "utils/http_utils.h"

esp_err_t get_timers_rest(httpd_req *req) {
    auto index = extract_index_from_uri(req->uri);

    std::array<char, 512> buf;
    auto result = get_timers_action(index, buf.data(), buf.size());

    if (result.answer_len) {
        send_in_chunks(req, buf, result.answer_len);
    } else {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "An error happened");
    }

    return ESP_OK;
}

esp_err_t post_timer_rest(httpd_req *req) {
    auto index = extract_index_from_uri(req->uri);

    std::array<char, 1024> buf;
    json_out answer = JSON_OUT_BUF(buf.data(), buf.size());
    size_t recv_size = std::min(req->content_len, buf.size());

    if (recv_size == 0) {
        json_printf(&answer, "{ %Q : %s}", "info", "No data provided");
        httpd_resp_sendstr(req, buf.data());

        return ESP_OK;
    }

    if (req->content_len > buf.size()) {
        json_printf(&answer, "{ %Q : %s}", "info", "Data too long");
        httpd_resp_sendstr(req, buf.data());
    
        return ESP_OK;
    }

    httpd_req_recv(req, buf.data(), recv_size);

    auto result = add_timer_action(index, buf.data(), recv_size);

    // TODO: check result
    httpd_resp_send(req, buf.data(), result.answer_len);

    return ESP_OK;
}

esp_err_t set_timer_rest(httpd_req *req) {
    auto index = extract_index_from_uri(req->uri);

    std::array<char, 1024> buf;
    json_out answer = JSON_OUT_BUF(buf.data(), buf.size());
    size_t recv_size = std::min(req->content_len, buf.size());

    if (!index) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Output index is not range");
        return ESP_OK;
    }

    if (recv_size == 0) {
        int answer_len = json_printf(&answer, "{ %Q : %s}", "info", "No data provided");
        httpd_resp_send(req, buf.data(), answer_len);

        return ESP_OK;
    }

    if (req->content_len > buf.size()) {
        int answer_len = json_printf(&answer, "{ %Q : %s}", "info", "Data too long");
        httpd_resp_send(req, buf.data(), answer_len);
    
        return ESP_OK;
    }

    httpd_req_recv(req, buf.data(), recv_size);

    auto result = set_timer_action(*index, buf.data(), buf.size());

    // TODO: check result
    httpd_resp_send(req, buf.data(), result.answer_len);

    return ESP_OK;
}

esp_err_t remove_timer_rest(httpd_req *req) {
    auto index = extract_index_from_uri(req->uri);
    std::array<char, 512> buf;

    if (!index) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Output index is not range");
        return ESP_OK;
    }

    auto result = remove_timer_action(*index, buf.data(), buf.size());

    // TODO: check result
    httpd_resp_send(req, buf.data(), result.answer_len);

    return ESP_OK;
}