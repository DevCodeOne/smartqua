#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <optional>
#include <thread>
#include <chrono>
#include <string_view>
#include <type_traits>

#include "esp_http_server.h"

#include "utils/stack_string.h"
#include "build_config.h"

std::optional<unsigned int> extract_index_from_uri(const char *uri);

namespace DetailsHelper {
    template<typename T, typename SFINAE = void>
    struct ReadToValue;

    template<typename T>
    struct ReadToValue<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
        static bool convert(const char *str, size_t size, T &dest) { 
            auto [ptr, result] = std::from_chars(str, str + size, dest);
            return result == std::errc();
        }
    };

};

// TODO: add transform method to transform string to type
template<typename DestType>
int getHeaderValue(httpd_req *req, const char *field, DestType dest, size_t len) {
    const auto headerValueSize = httpd_req_get_hdr_value_len(req, field);

    if (!headerValueSize || headerValueSize > len - 1) {
        return 0;
    }

    httpd_req_get_hdr_value_str(req, field, dest, len);
    return headerValueSize;
}

template<typename DestType>
bool getHeaderValue(httpd_req *req, const char *field, DestType &dest) {
    auto headerBuffer = SmallerBufferPoolType::get_free_buffer();
    const auto headerValueSize = httpd_req_get_hdr_value_len(req, field);

    if (!headerValueSize) {
        return false;
    }

    httpd_req_get_hdr_value_str(req, field, headerBuffer->data(), headerBuffer->size());
    return DetailsHelper::ReadToValue<DestType>::convert(headerBuffer->data(), headerValueSize, dest);
}

inline esp_err_t send_in_chunks(httpd_req *req, const char *chunk, int32_t num_bytes_to_send) {
    Logger::log(LogLevel::Info, "send_in_chunks");
    constexpr int32_t max_bytes_to_send = 1024;
    int32_t index = 0;

    httpd_resp_set_hdr(req, "Transfer-Encoding", "chunked");

    while (index < num_bytes_to_send) {
        auto to_send = std::min<int32_t>(std::min<int32_t>(num_bytes_to_send, num_bytes_to_send - index), max_bytes_to_send);
        auto result = httpd_resp_send_chunk(req, chunk + index, to_send);

        if (result != ESP_OK && result != 0x104) {
            Logger::log(LogLevel::Info, "Error with %d %d, %x", to_send, static_cast<int>(result), static_cast<int>(result));
            // Abort sending chunks
            httpd_resp_send_chunk(req, nullptr, 0);
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

    BasicStackString<512> link;
    BasicStackString<256> name;

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