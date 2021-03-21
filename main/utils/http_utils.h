#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <optional>

#include "esp_http_server.h"
#include "esp_log.h"

std::optional<unsigned int> extract_index_from_uri(const char *uri);

inline esp_err_t send_in_chunks(httpd_req *req, char *chunk, size_t num_bytes_to_send) {
    ESP_LOGI("http_utils", "send_in_chunks");
    int32_t left_to_send = num_bytes_to_send;
    int32_t index = 0;
    while (index < left_to_send) {
        int32_t to_send = std::min<int32_t>(num_bytes_to_send, left_to_send - index);
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
