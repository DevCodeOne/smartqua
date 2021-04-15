#pragma once

#include <cstdio>
#include <string_view>

#include "esp_http_server.h"
#include "esp_log.h"

#include "utils/web_utils.h"
#include "utils/large_buffer_pool.h"
#include "aq_main.h"

template <typename T = void>
class webserver final {
   public:
    webserver() {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.stack_size = 4096 * 4;
        config.uri_match_fn = httpd_uri_match_wildcard;
        config.max_uri_handlers = 16;

        httpd_start(&m_http_server_handle, &config);

    }

    webserver(const webserver &other) = delete;
    webserver(webserver &&other) = delete;

    // TODO: close server
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
    // TODO: get credentials from central settings
    if (!handle_authentication(req, "admin", "admin")) {
        return ESP_OK;
    }

    struct stat tmp_stat{};
    char filepath[256]{"/external/app_data"};
    std::string_view selected_path = "";
    std::string_view request_uri = req->uri;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // TODO: Remove trailing /
    if (request_uri == "/") {
        strncat(filepath, "/index.html", sizeof(filepath) - 1);
    } else {
        strncat(filepath, request_uri.data(), sizeof(filepath) - 1);
    }

    if (stat(filepath, &tmp_stat) != -1) {
        selected_path = filepath;
    }

    // Execute only if path wasn't already selected
    if (selected_path.size() == 0) {
        std::memset(reinterpret_cast<char *>(&tmp_stat), 0, sizeof(struct stat));
        if (stat(request_uri.data(), &tmp_stat) != -1) {
            selected_path = req->uri ;
        }
    }

    if (selected_path.size() == 0) {
        ESP_LOGE(__PRETTY_FUNCTION__, "Failed to open file or directory : %s", req->uri);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Path doesn't exist");
        return ESP_FAIL;
    }

    if (S_ISDIR(tmp_stat.st_mode)) {
        ESP_LOGI(__PRETTY_FUNCTION__, "Path is a directory %s, %s", selected_path.data(), request_uri.data());
        // TODO: maybe add pagination for more results witzh telldir and seekdir
        // TODO: replace closedir with something else, gsl::finally like
        DIR *directory = opendir(selected_path.data());
        dirent *current_entry = nullptr;
        long int entry_offset = 0;
        bool generated_path_directory = false;

        if (directory == nullptr) {
            ESP_LOGE(__PRETTY_FUNCTION__, "Failed to open directory %s", selected_path.data());
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Failed open directory");
            return ESP_FAIL;
        }

        auto buffer = large_buffer_pool_type::get_free_buffer();

        if (!buffer.has_value()) {
            ESP_LOGE(__PRETTY_FUNCTION__, "Failed to get buffer");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Failed to read existing file");
            closedir(directory);
            return ESP_FAIL;
        }

        auto gen_link = [&directory, &current_entry, &selected_path, &generated_path_directory](auto &link, auto &name) {
            if (!generated_path_directory) {
                copy_parent_directory(selected_path.data(), link.data(), link.size());
                std::strncpy(name.data(), "..", name.size());
                generated_path_directory = true;
                return true;
            }

            if (current_entry = readdir(directory); current_entry == nullptr) {
                return false;
            }
            std::snprintf(link.data(), link.size(), "%s/%s", selected_path.data(), current_entry->d_name);
            std::strncpy(name.data(), current_entry->d_name, name.size());

            return true;
        };

        int written_bytes = generate_link_list_website(buffer->data(), buffer->size(), gen_link);

        closedir(directory);

        if (written_bytes != -1) {
            send_in_chunks(req, buffer->data(), written_bytes);
        } else {
            ESP_LOGE(__PRETTY_FUNCTION__, "Buffer was too small for website");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Buffer was too small for website");
            return ESP_FAIL;;
        }
        
    } else if (S_ISREG(tmp_stat.st_mode)) {
        ESP_LOGI(__PRETTY_FUNCTION__, "Path is a file %s %s", selected_path.data(), request_uri.data());
        int fd = open(selected_path.data(), O_RDONLY, 0);

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
    }
    return ESP_OK;
}
