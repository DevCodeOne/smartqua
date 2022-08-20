#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <optional>
#include <thread>
#include <chrono>
#include <string_view>

#include "esp_http_server.h"

#include "utils/stack_string.h"
#include "smartqua_config.h"

std::optional<unsigned int> extract_index_from_uri(const char *uri);

inline esp_err_t send_in_chunks(httpd_req *req, char *chunk, int32_t num_bytes_to_send) {
    Logger::log(LogLevel::Info, "send_in_chunks");
    int32_t left_to_send = num_bytes_to_send;
    int32_t index = 0;
    while (index < left_to_send) {
        auto to_send = std::min<int32_t>(num_bytes_to_send, left_to_send - index);
        auto result = httpd_resp_send_chunk(req, chunk + index, to_send);

        if (result != ESP_OK) {
            // Abort sending chunks
            httpd_resp_sendstr_chunk(req, nullptr);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send remaining chunks");
            return result;
        }
        index += to_send;
    }

    httpd_resp_send_chunk(req, nullptr, 0);

    return ESP_OK;
   
}

template<size_t BufferSize, size_t ChunkSize = 512>
esp_err_t send_in_chunks(httpd_req *req, std::array<char, BufferSize> &chunk, size_t num_bytes_to_send = BufferSize) {
    return send_in_chunks(req, chunk.data(), num_bytes_to_send);
}

template<typename GeneratorFunction>
int generate_link_list_website(char *dst, size_t dst_size, GeneratorFunction &gen_func) {
    int offset = 0;
    int tmp_offset = 0;
    tmp_offset = snprintf(dst, dst_size,
    R"(
    <html>
        <body>
            <ul>
    )");

    if (tmp_offset < 0 || tmp_offset >= dst_size - 1) {
        return -1;
    }
    
    offset += tmp_offset;

    stack_string<512> link;
    stack_string<256> name;

    while (gen_func(link, name)) {
        tmp_offset = snprintf(dst + offset, dst_size - offset,
        R"(
                <li><a href="%s">%s</a></li>
        )",
        link.data(), name.data());

        if (tmp_offset < 0 || tmp_offset >= dst_size - offset) {
            return -1;
        }
        
        offset += tmp_offset;
    }
    tmp_offset = snprintf(dst + offset, dst_size - offset, R"(
            </ul>
        </body>
    </html>
    )");

    if (tmp_offset < 0 || tmp_offset >= dst_size - offset) {
        return -1;
    }

    offset += tmp_offset;

    return offset;
}

bool handle_authentication(httpd_req *req, std::string_view user, std::string_view password);