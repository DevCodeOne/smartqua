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
    std::array<char, 2048> buf;
    json_action_result result{};
    auto [complete_match, index, what] = ctre::match<pattern>(req->uri);

    if (req->content_len > buf.size()) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Input too long");
    }

    httpd_req_recv(req, buf.data(), req->content_len);

    if (index) {
        auto index_value = std::atoi(index.to_view().data());
        
        if (req->method == HTTP_GET && !what) {
            result = get_devices_action(index_value, buf.data(), req->content_len, buf.data(), buf.size());
        } else if (req->method == HTTP_GET && what) { // TODO: check what what really is
            result = get_device_info(index_value, buf.data(), req->content_len, buf.data(), buf.size());
        } else if (req->method == HTTP_PUT) {
            result = add_device_action(index_value, buf.data(), req->content_len, buf.data(), buf.size());
        } else if (req->method == HTTP_DELETE) {
            result = remove_device_action(index_value, buf.data(), req->content_len, buf.data(), buf.size());
        } else if (req->method == HTTP_PATCH) {
            result = set_device_action(index_value, buf.data(), req->content_len, buf.data(), buf.size());
        }

    } else if (!index && (req->method == HTTP_POST || req->method == HTTP_GET)) {
        if (req->method == HTTP_GET) {
            result = get_devices_action(std::nullopt, buf.data(), req->content_len, buf.data(), buf.size());
        } else if (req->method == HTTP_POST) {
            result = add_device_action(std::nullopt, buf.data(), req->content_len, buf.data(), buf.size());
        }
    }

    if (result.answer_len) {
        send_in_chunks(req, buf, result.answer_len);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "An error happened");
    }

    return ESP_OK;
}