#include "devices_rest.h"

#include <cstdio>
#include <cstring>
#include <string_view>
#include <algorithm>
#include <thread>

#include "actions/action_types.h"
#include "smartqua_config.h"
#include "actions/device_actions.h"
#include "esp_http_server.h"
#include "utils/esp/esp_filesystem_utils.h"
#include "utils/logger.h"
#include "utils/esp/web_utils.h"
#include "build_config.h"

#include "runtime_access.h"

#include "ctre.hpp"

enum struct ContentType {
    Default, Binary
};

static constexpr ctll::fixed_string pattern{R"(\/api\/v1\/devices\/(?<index>[0-9]+)(?:\/(?<what>[\w\-]+)|\/)?)"};

// TODO: backup functionality has be in a different api slot
// TODO: Run actions in main thread, so no stack overflows happen
esp_err_t do_devices(httpd_req *req) {
    Logger::log(LogLevel::Info, "This thread id %d", std::this_thread::get_id());
    Logger::log(LogLevel::Info, "Handle uri %s", req->uri);
    auto buffer = LargeBufferPoolType::get_free_buffer();

    if (!buffer) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_OK;
    }

    JsonActionResult result{};
    auto [complete_match, index, what] = ctre::match<pattern>(req->uri);

    Logger::log(LogLevel::Debug, "Receiving input for device");
    if (req->content_len < buffer->size()) {
        httpd_req_recv(req, buffer->data(), req->content_len);
    } else {
        // This is too large for the buffer, this has to be received chunk-wise
        Logger::log(LogLevel::Info, "Has to be received chunk-wise");
        // httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Input too long, not handled yet ...");
    }

    auto headerParamBuffer = LargeBufferPoolType::get_free_buffer();
    auto acceptLen = getHeaderValue<char *>(req, "Accept", headerParamBuffer->data(), headerParamBuffer->size());
    auto acceptType = ContentType::Default;

    if (acceptLen) {
        // TODO: improve this somehow 
        if (std::string_view acceptTypeStr(headerParamBuffer->data(), acceptLen); 
                acceptTypeStr.compare("application/octet-stream") == 0) {
            acceptType = ContentType::Binary;
        } 
        Logger::log(LogLevel::Info, "Accept : %.*s ", acceptLen, headerParamBuffer->data());
    }

    auto contentLen = getHeaderValue<char *>(req, "Content-Type", headerParamBuffer->data(), headerParamBuffer->size());
    auto contentType = ContentType::Default; 

    if (contentLen) {
        if (std::string_view contentTypeStr(headerParamBuffer->data(), contentLen);
                contentTypeStr.compare("application/octet-stream") == 0) {
            contentType = ContentType::Binary;
        }
        Logger::log(LogLevel::Info, "Content-Type : %.*s ", contentLen, headerParamBuffer->data());
    }

    unsigned int contentLength = 0;
    getHeaderValue<unsigned int>(req, "Content-Length", contentLength);
    Logger::log(LogLevel::Info, "Content-Length : %u", contentLength);

    if (index) {
        Logger::log(LogLevel::Debug, "Executing action for device");
        auto index_value = std::atoi(index.to_view().data());

        if (req->method == HTTP_GET) {
                // TODO: check what what really is
            const std::string_view whatView = what.to_view();
            if (whatView.starts_with("/info")) {
                result = get_device_info(index_value, whatView.data(), whatView.size(),
                                         buffer->data(), buffer->size());
            } else {
                result = get_devices_action(index_value, whatView.data(), whatView.size(),
                buffer->data(), buffer->size());
            }
        } else if (req->method == HTTP_PUT && !what) {
            result = add_device_action(index_value, buffer->data(), req->content_len, buffer->data(), buffer->size());
        } else if (req->method == HTTP_PUT && what) {
            result = write_device_options_action(index_value, what.to_view().data(), buffer->data(), req->content_len, buffer->data(), buffer->size());
        } else if (req->method == HTTP_DELETE) {
            result = remove_device_action(index_value, buffer->data(), req->content_len, buffer->data(), buffer->size());
        } else if (req->method == HTTP_PATCH) {
            if (what) {
                result = set_device_action(index_value, what.to_view().data(), buffer->data(), req->content_len, buffer->data(), buffer->size());
            } else {
                result = set_device_action(index_value, std::string_view("", 0), buffer->data(), req->content_len, buffer->data(), buffer->size());
            }
        }

    } else if (!index && (req->method == HTTP_POST || req->method == HTTP_GET)) {
        if (req->method == HTTP_GET) {
            if (acceptType == ContentType::Binary) {
                auto mappedPartition = mapPartition<DeviceKind::ESPDevice>("values");

                if (!mappedPartition) {
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "An error happened");
                    return ESP_OK;
                }

                httpd_resp_set_type(req, "application/octet-stream");
                send_in_chunks(req, reinterpret_cast<const char *>(mappedPartition->data()), static_cast<uint32_t>(mappedPartition->size()));
                return ESP_OK;
            }

            result = get_devices_action(std::nullopt, buffer->data(), req->content_len, buffer->data(), buffer->size());
        } else if (req->method == HTTP_POST) {
            if (contentType == ContentType::Binary) {
                Logger::log(LogLevel::Info, "Getting partition backup");

                auto dataSource = [&req](auto &bufferType) -> size_t {
                    auto receiveResult = httpd_req_recv(req, bufferType->data(), bufferType->size());

                    if (receiveResult > 0) {
                        return receiveResult;
                    }

                    // TODO: handle error somehow
                    return 0;
                };

                // TODO: check Content-Length for partition size first
                bool result = DefaultStorage::unMountWriteBackupAndMount(dataSource);

                if (result) {
                    httpd_resp_set_status(req, HTTPD_200);
                    httpd_resp_sendstr(req, "Ok wrote partition to flash");
                } else {
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Couldn't write backup to partition");
                }
                return ESP_OK;
            } else {
                Logger::log(LogLevel::Info, "Content-Type is non binary");
            }

            result = add_device_action(std::nullopt, buffer->data(), req->content_len, buffer->data(), buffer->size());
        }
    }

    Logger::log(LogLevel::Debug, "Sending response from device");
    if (result.answer_len && buffer->data()[0] != '\0') {
        httpd_resp_set_type(req, "application/json");
        send_in_chunks(req, buffer->data(), result.answer_len);
    } else if (result.answer_len == 0 && result.result == JsonActionResultStatus::success) {
        httpd_resp_set_status(req, HTTPD_204);
        httpd_resp_send(req, "", 0);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "An error happened");
    }
    return ESP_OK;
}