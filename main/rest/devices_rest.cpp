#include "devices_rest.h"

#include <cstring>
#include <algorithm>

#include "aq_main.h"
#include "actions/device_actions.h"
#include "drivers/device_types.h"
#include "utils/http_utils.h"

#include "ctre.hpp"

static constexpr ctll::fixed_string pattern{R"(\/api\/v1\/devices\/(?<index>[0-9]+)(?:\/(?<what>\w+)|\/)?)"};

esp_err_t do_devices(httpd_req *req) {
    ESP_LOGI("Devices_Rest", "Handle uri %s", req->uri);
    auto buffer = large_buffer_pool_type::get_free_buffer();

    if (!buffer) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_OK;
    }

    json_action_result result{};
    auto [complete_match, index, what] = ctre::match<pattern>(req->uri);

    if (req->content_len > buffer->size()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Input too long");
        return ESP_OK;
    }

    httpd_req_recv(req, buffer->data(), req->content_len);

    if (index) {
        auto index_value = std::atoi(index.to_view().data());
        
        if (req->method == HTTP_GET && !what) {
            result = get_devices_action(index_value, buffer->data(), req->content_len, buffer->data(), buffer->size());
        } else if (req->method == HTTP_GET && what) { // TODO: check what what really is
            result = get_device_info(index_value, buffer->data(), req->content_len, buffer->data(), buffer->size());
        } else if (req->method == HTTP_PUT) {
            result = add_device_action(index_value, buffer->data(), req->content_len, buffer->data(), buffer->size());
        } else if (req->method == HTTP_DELETE) {
            result = remove_device_action(index_value, buffer->data(), req->content_len, buffer->data(), buffer->size());
        } else if (req->method == HTTP_PATCH) {
            result = set_device_action(index_value, buffer->data(), req->content_len, buffer->data(), buffer->size());
        }

    } else if (!index && (req->method == HTTP_POST || req->method == HTTP_GET)) {
        if (req->method == HTTP_GET) {
            result = get_devices_action(std::nullopt, buffer->data(), req->content_len, buffer->data(), buffer->size());
        } else if (req->method == HTTP_POST) {
            result = add_device_action(std::nullopt, buffer->data(), req->content_len, buffer->data(), buffer->size());
        }
    }

    if (result.answer_len) {
        send_in_chunks(req, buffer->data(), result.answer_len);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "An error happened");
    }
    return ESP_OK;
}