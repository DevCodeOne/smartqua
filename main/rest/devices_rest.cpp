#include "devices_rest.h"

#include <cstring>
#include <algorithm>

#include "aq_main.h"
#include "utils/http_utils.h"
#include "actions/device_actions.h"
#include "drivers/device_types.h"

esp_err_t get_devices(httpd_req *req) {
    auto index = extract_index_from_uri(req->uri);

    std::array<char, 512> buf;
    auto result = get_devices_action(index, buf.data(), buf.size());

    if (result.answer_len) {
        send_in_chunks(req, buf, result.answer_len);
    } else {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "An error happened");
    }

    return ESP_OK;
}

esp_err_t post_device(httpd_req *req) {
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

    auto result = add_device_action(index, buf.data(), recv_size);

    // TODO: check result
    httpd_resp_send(req, buf.data(), result.answer_len);

    return ESP_OK;
}

esp_err_t set_device(httpd_req *req) {
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

    auto result = set_device_action(*index, buf.data(), buf.size());

    // TODO: check result
    httpd_resp_send(req, buf.data(), result.answer_len);

    return ESP_OK;
}

esp_err_t remove_device(httpd_req *req) {
    auto index = extract_index_from_uri(req->uri);
    std::array<char, 512> buf;

    if (!index) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Output index is not range");
        return ESP_OK;
    }

    auto result = remove_device_action(*index, buf.data(), buf.size());

    // TODO: check result
    httpd_resp_send(req, buf.data(), result.answer_len);

    return ESP_OK;
}