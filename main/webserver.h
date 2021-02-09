#pragma once

#include <cstdio>
#include <string_view>

#include "esp_http_server.h"
#include "esp_log.h"

template <typename T = void>
class webserver final {
   public:
    webserver() {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.stack_size = 4096 * 2;
        config.uri_match_fn = httpd_uri_match_wildcard;

        httpd_start(&m_http_server_handle, &config);

    }

    webserver(const webserver &other) = delete;
    webserver(webserver &&other) = delete;

    ~webserver() {}

    webserver &operator=(const webserver &other) = delete;
    webserver &operator=(webserver &&other) = delete;

    void register_file_handler() {
        httpd_uri_t file_handler = {.uri = "/*",
                                    .method = HTTP_GET,
                                    .handler = webserver::get_file,
                                    .user_ctx = this};

        register_handler(file_handler);
    }

    void register_handler(httpd_uri_t handler) {
        httpd_register_uri_handler(m_http_server_handle, &handler);
    }

   private:
    static esp_err_t get_file(httpd_req_t *req);

    httpd_handle_t m_http_server_handle = nullptr;
};

inline esp_err_t set_content_type_from_file(httpd_req_t *req,
                                            const char *filepath) {
    auto ends_with = [](std::string_view current_view, std::string_view ending) {
        const auto view_len = current_view.length();
        const auto ending_len = ending.length();
        if (view_len < ending_len) {
            return false;
        }

        return current_view.substr(view_len - ending_len) == ending;
    };

    const char *type = "text/plain";
    if (ends_with(filepath, ".html")) {
        type = "text/html";
    } else if (ends_with(filepath, ".js")) {
        type = "application/javascript";
    } else if (ends_with(filepath, ".css")) {
        type = "text/css";
    } else if (ends_with(filepath, ".png")) {
        type = "image/png";
    } else if (ends_with(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (ends_with(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

template <typename T>
esp_err_t webserver<T>::get_file(httpd_req_t *req) {
    char filepath[256]{"/storage"};
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (req->uri[strlen(req->uri) - 1] == '/') {
        strncat(filepath, "/index.html", sizeof(filepath) - 1);
    } else {
        strncat(filepath, req->uri, sizeof(filepath) - 1);
    }

    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(__PRETTY_FUNCTION__, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char chunk[512];
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, sizeof(chunk));
        if (read_bytes == -1) {
            ESP_LOGE(__PRETTY_FUNCTION__, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(__PRETTY_FUNCTION__, "File sending failed! %s", filepath);
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, nullptr);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                    "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(__PRETTY_FUNCTION__, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, nullptr, 0);
    return ESP_OK;
}
