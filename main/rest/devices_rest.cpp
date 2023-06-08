#include "devices_rest.h"

#include <cstring>
#include <algorithm>
#include <thread>

#include "actions/action_types.h"
#include "smartqua_config.h"
#include "actions/device_actions.h"
#include "drivers/device_types.h"
#include "esp_http_server.h"
#include "utils/logger.h"
#include "utils/web_utils.h"
#include "build_config.h"

#include "ctre.hpp"

static constexpr ctll::fixed_string pattern{R"(\/api\/v1\/devices\/(?<index>[0-9]+)(?:\/(?<what>[\w\-]+)|\/)?)"};

esp_err_t do_devices(httpd_req *req) {
    Logger::log(LogLevel::Info, "This thread id %d", std::this_thread::get_id());
    Logger::log(LogLevel::Info, "Handle uri %s", req->uri);
    auto buffer = LargeBufferPoolType::get_free_buffer();

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

    Logger::log(LogLevel::Debug, "Receiving input for device");
    httpd_req_recv(req, buffer->data(), req->content_len);

    Logger::log(LogLevel::Debug, "Executing action for device");
    if (index) {
        auto index_value = std::atoi(index.to_view().data());
        
        if (req->method == HTTP_GET && !what) {
            json_token param{};
            json_scanf(buffer->data(), req->content_len, "{ param : %T }", &param);
            result = get_devices_action(index_value, param.ptr, param.len, buffer->data(), buffer->size());
        } else if (req->method == HTTP_GET && what) { // TODO: check what what really is
            result = get_device_info(index_value, buffer->data(), req->content_len, buffer->data(), buffer->size());
        } else if (req->method == HTTP_PUT && !what) {
            result = add_device_action(index_value, buffer->data(), req->content_len, buffer->data(), buffer->size());
        } else if (req->method == HTTP_PUT && what) {
            result = write_device_options_action(index_value, what.to_view().data(), buffer->data(), req->content_len, buffer->data(), buffer->size());
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

    Logger::log(LogLevel::Debug, "Sending response from device");
    if (result.answer_len && buffer->data()[0] != '\0') {
        httpd_resp_set_type(req, "application/json");
        send_in_chunks(req, buffer->data(), result.answer_len);
    } else if (result.answer_len == 0 && result.result == json_action_result_value::successfull) {
        httpd_resp_set_status(req, HTTPD_204);
        httpd_resp_send(req, "", 0);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "An error happened");
    }
    return ESP_OK;
}