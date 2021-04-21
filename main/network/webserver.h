#pragma once

#include <cstdio>
#include <string_view>
#include <memory>

#include "esp_https_server.h"
#include "esp_log.h"

#include "utils/web_utils.h"
#include "utils/large_buffer_pool.h"
#include "utils/ssl_credentials.h"
#include "aq_main.h"

enum struct security_level {
    unsecured, secured
};

namespace Detail {
    template<security_level level>
    class webserver_handle;

    template<>
    class webserver_handle<security_level::unsecured> {
        public:
            webserver_handle() = default;
            ~webserver_handle() { stop_server(); }
            operator httpd_handle_t() const { return m_http_server_handle; }

            bool init_server() {
                httpd_config_t config = HTTPD_DEFAULT_CONFIG();
                config.uri_match_fn = httpd_uri_match_wildcard;
                config.max_uri_handlers = 16;
                config.stack_size = 3 * 4096;

                return httpd_start(&m_http_server_handle, &config) == ESP_OK;
            }

            bool stop_server() {
                httpd_stop(m_http_server_handle);

                return true;
            }

        private:
            httpd_handle_t m_http_server_handle = nullptr;
    };

    template<>
    class webserver_handle<security_level::secured> {
        public:
            webserver_handle(const char *cert_path, const char *key_path);
            ~webserver_handle();
            operator httpd_handle_t() const { return m_http_server_handle; }

            bool init_server();
            bool stop_server();

        private:
            httpd_handle_t m_http_server_handle = nullptr;
            ssl_credentials<2048, 2048> m_credentials;
    };

    webserver_handle<security_level::secured>::webserver_handle(const char *cert_path, const char *key_path) : m_credentials(cert_path, key_path) {}

    webserver_handle<security_level::secured>::~webserver_handle() {
        stop_server();
    }

    bool webserver_handle<security_level::secured>::init_server() {
        httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
        config.httpd.uri_match_fn = httpd_uri_match_wildcard;
        config.httpd.max_uri_handlers = 16;
        config.httpd.ctrl_port = 1024;

        if (m_credentials) {
            ESP_LOGI("Webserver", "Found SSL credentials trying to start a https server");
            config.cacert_pem = reinterpret_cast<const uint8_t *>(m_credentials.cert_begin());
            config.cacert_len = m_credentials.cert_len();
            config.prvtkey_pem = reinterpret_cast<const uint8_t *>(m_credentials.key_begin());
            config.prvtkey_len = m_credentials.key_len();
            config.httpd.max_open_sockets = 1;
        } else {
            ESP_LOGI("Webserver", "SSL credentials weren't found falling back to http server");
            config.transport_mode = HTTPD_SSL_TRANSPORT_INSECURE;
        }

        return httpd_ssl_start(&m_http_server_handle, &config) == ESP_OK;
    }

    bool webserver_handle<security_level::secured>::stop_server() {
        httpd_ssl_stop(&m_http_server_handle);
        return true;
    }

}

template <security_level level = security_level::secured>
class webserver final {
   public:
    static constexpr char file_handler_uri_path[] = "/*";

    webserver(const char *base_path = "/");
    webserver(const webserver &other) = delete;
    webserver(webserver &&other) = delete;
    ~webserver() = default;

    webserver &operator=(const webserver &other) = delete;
    webserver &operator=(webserver &&other) = delete;

    void register_handler(httpd_uri_t handler) {
        httpd_unregister_uri_handler(m_server_handle, file_handler_uri_path, HTTP_GET);

        httpd_register_uri_handler(m_server_handle, &handler);
        // File handler always has to be registered last
        register_file_handler();
    }

   private:
    void register_file_handler() {
        httpd_uri_t file_handler = {
            .uri = file_handler_uri_path,
            .method = HTTP_GET,
            .handler = webserver::get_file,
            .user_ctx = reinterpret_cast<void*>(this)};
        httpd_register_uri_handler(m_server_handle, &file_handler);
    }

    static esp_err_t get_file(httpd_req_t *req);

    Detail::webserver_handle<level> m_server_handle{};
    const char *m_base_path = nullptr;
};

template<>
webserver<security_level::secured>::webserver(const char *base_path) : 
        m_server_handle("/external/app_data/certs/cert.pem", "/external/app_data/certs/key.pem"),
        m_base_path(base_path) {
    m_server_handle.init_server();
    register_file_handler();
}

template<>
webserver<security_level::unsecured>::webserver(const char *base_path) : 
        m_server_handle(),
        m_base_path(base_path) {
    m_server_handle.init_server();
    register_file_handler();
}

// TODO: maybe replace with regex
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

template <security_level level>
esp_err_t webserver<level>::get_file(httpd_req_t *req) {
    // TODO: get credentials from central settings
    // Don't check for credentials when the server runs in unsecure mode to avoid leaking passwords
    if (level == security_level::secured && !handle_authentication(req, "admin", "admin")) {
        return ESP_OK;
    }

    const char *base_path = static_cast<webserver *>(req->user_ctx)->m_base_path;

    if (base_path == nullptr) {
        ESP_LOGE(__PRETTY_FUNCTION__, "Basepath isn't configured");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Basepath isn't configured");
        return ESP_FAIL;
    }

    struct stat tmp_stat{};
    std::array<char, 256> filepath;
    std::string_view selected_path = "";
    std::string_view request_uri = req->uri;

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Connection", "Keep-Alive timeout=1, max=1000");


    if (base_path != nullptr) {
        std::strncpy(filepath.data(), base_path, filepath.size());
    }

    // TODO: Remove trailing /
    if (request_uri == "/") {
        strncat(filepath.data(), "/index.html", sizeof(filepath) - 1);
    } else {
        strncat(filepath.data(), request_uri.data(), sizeof(filepath) - 1);
    }

    if (stat(filepath.data(), &tmp_stat) < 0) {
        ESP_LOGE(__PRETTY_FUNCTION__, "Failed to open file or directory : %s", filepath.data());
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Path doesn't exist");
        return ESP_FAIL;
    }

    selected_path = filepath.data();

    auto buffer = large_buffer_pool_type::get_free_buffer();

    if (!buffer.has_value()) {
        ESP_LOGE(__PRETTY_FUNCTION__, "Failed to get buffer");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Failed to read existing file");
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
            ESP_LOGE(__PRETTY_FUNCTION__, "Failed to open file : %s", filepath.data());
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Failed to read existing file");
            return ESP_FAIL;
        }


        set_content_type_from_file(req, filepath.data());

        ssize_t read_bytes;
        // TODO: make configurable
        auto max_chunk_size = std::min<size_t>(buffer->size(), 4096);
        do {
            /* Read file in chunks into the scratch buffer */
            read_bytes = read(fd, buffer->data(), max_chunk_size);
            if (read_bytes == -1) {
                ESP_LOGE(__PRETTY_FUNCTION__, "Failed to read file : %s", filepath.data());
            } else if (read_bytes > 0) {
                /* Send the buffer contents as HTTP response chunk */
                esp_err_t last_result = ESP_FAIL;
                // TODO: make configurable 
                for (unsigned int i = 0; i < 2 && last_result != ESP_OK; ++i) {
                    last_result = httpd_resp_send_chunk(req, buffer->data(), read_bytes);

                    if (last_result != ESP_OK) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                    }
                }

                if (last_result != ESP_OK) {
                    close(fd);
                    ESP_LOGE(__PRETTY_FUNCTION__, "File sending failed! %s", filepath.data());
                    /* Abort sending file */
                    httpd_resp_send_chunk(req, nullptr, 0);
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
