#pragma once

#include <esp_err.h>

#include <cstdio>
#include <cstring>
#include <pthread.h>

#include <string_view>
#include <memory>

#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

// TODO: configure via esp settings
#if defined(CONFIG_ESP_HTTPS_SERVER_ENABLE) && CONFIG_ESP_HTTPS_SERVER_ENABLE == y
#define ENABLE_HTTPS
#include "esp_https_server.h"
#endif
#include "esp_http_server.h"
#include "esp_vfs_fat.h"

#include "utils/web_utils.h"
#include "utils/large_buffer_pool.h"
#include "utils/ssl_credentials.h"
#include "utils/logger.h"
#include "build_config.h"

enum struct WebServerSecurityLevel {
    unsecured,
    #ifdef ENABLE_HTTPS
    secured
    #endif
};

namespace Detail {
    template<WebServerSecurityLevel level>
    class WebServerHandle;

    template<>
    class WebServerHandle<WebServerSecurityLevel::unsecured> {
        public:
            WebServerHandle() = default;
            ~WebServerHandle() { stopServer(); }
            explicit operator httpd_handle_t() const { return m_http_server_handle; }

            bool initServer() {
                httpd_config_t config = HTTPD_DEFAULT_CONFIG();
                config.uri_match_fn = httpd_uri_match_wildcard;
                config.stack_size = 4096;
                config.keep_alive_enable = false;
                config.lru_purge_enable = true;
                config.recv_wait_timeout = 5;
                config.send_wait_timeout = 5;
                config.enable_so_linger = false;
                config.linger_timeout = 5;
                config.core_id = 1;
                config.max_uri_handlers = 6;
                config.max_open_sockets = 1;
                config.recv_wait_timeout = 30;
                config.send_wait_timeout = 30;

                esp_err_t serverStart;
                if ((serverStart = httpd_start(&m_http_server_handle, &config)) != ESP_OK) {
                    switch (serverStart) {
                        case ESP_ERR_INVALID_ARG:
                            Logger::log(LogLevel::Error, "Couldn't start webserver (invalid arguments)");
                            break;
                        case ESP_ERR_HTTPD_ALLOC_MEM:
                            Logger::log(LogLevel::Error, "Couldn't start webserver (couldn't alloc mem)");
                            break;
                        case ESP_ERR_HTTPD_TASK:
                            Logger::log(LogLevel::Error, "Couldn't start webserver (couldn't launch server task)");
                            break;
                    }
                }

                return serverStart == ESP_OK;
            }

            bool stopServer() {
                httpd_stop(m_http_server_handle);

                return true;
            }

        private:
            httpd_handle_t m_http_server_handle = nullptr;
    };

    #ifdef ENABLE_HTTPS
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
            Logger::log(LogLevel::Info, "Found SSL credentials trying to start a https server");
            config.cacert_pem = reinterpret_cast<const uint8_t *>(m_credentials.cert_begin());
            config.cacert_len = m_credentials.cert_len();
            config.prvtkey_pem = reinterpret_cast<const uint8_t *>(m_credentials.key_begin());
            config.prvtkey_len = m_credentials.key_len();
            config.httpd.max_open_sockets = 1;
        } else {
            Logger::log(LogLevel::Info, "SSL credentials weren't found falling back to http server");
            config.transport_mode = HTTPD_SSL_TRANSPORT_INSECURE;
        }

        return httpd_ssl_start(&m_http_server_handle, &config) == ESP_OK;
    }

    bool webserver_handle<security_level::secured>::stop_server() {
        httpd_ssl_stop(&m_http_server_handle);
        return true;
    }
    #endif

}

template <WebServerSecurityLevel level = WebServerSecurityLevel::unsecured>
class WebServer final {
   private:
    static esp_err_t getFile(httpd_req_t *req);

    struct Handler final {
        std::string_view prefix = "";
        uint64_t methods = 0;
        esp_err_t (*handler)(httpd_req *req);
    };

   public:
    static constexpr char file_handler_uri_path[] = "/*";

    explicit WebServer(const char *base_path = "/");
    WebServer(const WebServer &other) = delete;
    WebServer(WebServer &&other) = delete;
    ~WebServer() = default;

    WebServer &operator=(const WebServer &other) = delete;
    WebServer &operator=(WebServer &&other) = delete;

    void registerHandler(std::string_view prefix, uint32_t methods, esp_err_t (*handler)(httpd_req *req)) {
        if (prefix.empty()) {
            return;
        }

        Logger::log(LogLevel::Info, "Registering handler with prefix %s", prefix.data());

        // The last pos is reserved for the filehandler
        auto firstFreeSlot = std::find(m_handler.begin(), m_handler.end() - 1, std::nullopt);

        if (firstFreeSlot == m_handler.end() - 1) {
            // TODO: handle error -> no empty slot found
        }

        *firstFreeSlot = Handler{
                .prefix = prefix,
                .methods = methods,
                .handler = handler
        };
    }

    // TODO: implement
    // void unregisterHandler() { }

    void registerFileHandler() {
        m_handler[m_handler.size() - 1] = Handler{
            .prefix = "",
            .methods = 1 << static_cast<uint8_t>(HTTP_GET),
            .handler = &getFile
        };
    } 

    private:
    void registerMainHandler() {
        const auto generateHandler = [this](const auto &method) {
            return httpd_uri_t{
                .uri = "*",
                .method = method,
                .handler = WebServer::mainHandler,
                .user_ctx = reinterpret_cast<void *>(this)
            };
        };
        const auto main_handler_get = generateHandler(HTTP_GET);
        const auto main_handler_post = generateHandler(HTTP_POST);
        const auto main_handler_put = generateHandler(HTTP_PUT);
        const auto main_handler_delete = generateHandler(HTTP_DELETE);
        const auto main_handler_patch = generateHandler(HTTP_PATCH);

        httpd_register_uri_handler(static_cast<httpd_handle_t>(m_server_handle),
                                   &main_handler_get);
        httpd_register_uri_handler(static_cast<httpd_handle_t>(m_server_handle),
                                   &main_handler_post);
        httpd_register_uri_handler(static_cast<httpd_handle_t>(m_server_handle),
                                   &main_handler_put);
        httpd_register_uri_handler(static_cast<httpd_handle_t>(m_server_handle),
                                   &main_handler_delete);
        httpd_register_uri_handler(static_cast<httpd_handle_t>(m_server_handle),
                                   &main_handler_patch);
        
    }

    struct HandlerThreadParam {
        esp_err_t (*handler)(httpd_req *req);
        httpd_req *req;
        esp_err_t result = ESP_FAIL;
    };

    static void *threadWrapper(void *handlerParam) {
        auto *const param = reinterpret_cast<HandlerThreadParam *>(handlerParam);
        param->result = param->handler(param->req);

        pthread_exit(nullptr);
    }

    static esp_err_t mainHandler(httpd_req *req) {
        std::string_view uri_view = req->uri;
        auto *thiz = reinterpret_cast<WebServer *>(req->user_ctx);

        // Delete user_ctx since this pointer shouldn't be used later on
        req->user_ctx = nullptr;
        auto const foundHandler = std::find_if(thiz->m_handler.cbegin(), thiz->m_handler.cend(),
                                         [&uri_view](const auto &currentHandler) {
                                             return currentHandler && uri_view.starts_with(currentHandler->prefix);
                                         });

        if (foundHandler == thiz->m_handler.cend()) {
            return ESP_FAIL;
        }

        Logger::log(LogLevel::Info, "Found handler with prefix : %s", (*foundHandler)->prefix.data());

        pthread_attr_t attributes;
        pthread_t newThreadHandle;

        pthread_attr_init(&attributes);
        pthread_attr_setstacksize(&attributes, stack_size);

        HandlerThreadParam param{
                .handler = (*foundHandler)->handler,
                .req = req,
                .result = ESP_FAIL
        };

        DoFinally cleanup([&attributes]() {
            pthread_attr_destroy(&attributes);
        });

        if (pthread_create(&newThreadHandle,
                           &attributes,
                           threadWrapper, (void *) &param) != 0) {
            return ESP_FAIL;
        }

        if (pthread_join(newThreadHandle, nullptr) != 0) {
            return ESP_FAIL;
        }

        return param.result;
    }
    
    Detail::WebServerHandle<level> m_server_handle{};
    const char *m_base_path = nullptr;
    std::array<std::optional<Handler>, 16> m_handler;
};

/*
template<>
webserver<security_level::secured>::webserver(const char *base_path) : 
        m_server_handle("/external/app_data/certs/cert.pem", "/external/app_data/certs/key.pem"),
        m_base_path(base_path) {
    m_server_handle.init_server();
    register_file_handler();
}*/

template<>
WebServer<WebServerSecurityLevel::unsecured>::WebServer(const char *base_path) : 
        m_server_handle(),
        m_base_path(base_path) {
    m_server_handle.initServer();
    registerMainHandler();
}

// TODO: maybe replace with regex
inline esp_err_t setContentTypeFromFile(httpd_req_t *req,
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

template <WebServerSecurityLevel level>
esp_err_t WebServer<level>::getFile(httpd_req_t *req) {
    // TODO: get credentials from central settings
    // Don't check for credentials when the server runs in unsecure mode to avoid leaking passwords
    if (level != WebServerSecurityLevel::unsecured && !handle_authentication(req, "admin", "admin")) {
        return ESP_OK;
    }

    const char *base_path = static_cast<WebServer *>(req->user_ctx)->m_base_path;

    if (base_path == nullptr) {
        Logger::log(LogLevel::Error, "Basepath isn't configured");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Basepath isn't configured");
        return ESP_FAIL;
    }

    struct stat tmp_stat{};
    std::array<char, 128> filepath{};
    std::string_view selected_path;
    std::string_view request_uri{req->uri};

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Connection", "Keep-Alive");
    httpd_resp_set_hdr(req, "Keep-Alive", "timeout=1, max=1000");

    std::strncpy(filepath.data(), base_path, filepath.size() - 1);

    // TODO: Remove trailing /
    if (request_uri == "/") {
        strncat(filepath.data(), "/index.html", sizeof(filepath) - 1);
    } else {
        strncat(filepath.data(), request_uri.data(), sizeof(filepath) - 1);
    }

    if (stat(filepath.data(), &tmp_stat) < 0) {
        Logger::log(LogLevel::Error, "Failed to open file or directory : %s", filepath.data());
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Path doesn't exist");
        return ESP_FAIL;
    }

    selected_path = filepath.data();

    auto buffer = LargeBufferPoolType::get_free_buffer();

    if (!buffer.has_value()) {
        Logger::log(LogLevel::Error, "Failed to get buffer");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Failed to read existing file");
        return ESP_FAIL;
    }

    if (S_ISDIR(tmp_stat.st_mode)) {
        Logger::log(LogLevel::Error, "Path is a directory %s, %s", selected_path.data(), request_uri.data());
        // TODO: maybe add pagination for more results witzh telldir and seekdir
        auto directory = opendir(selected_path.data());
        dirent *current_entry = nullptr;
        long int entry_offset = 0;
        bool generated_path_directory = false;
        DoFinally closeOp( [&directory]() {
            closedir(directory);
        });

        if (directory == nullptr) {
            Logger::log(LogLevel::Error, "Failed to open directory %s", selected_path.data());
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

        if (written_bytes != -1) {
            send_in_chunks(req, buffer->data(), written_bytes);
        } else {
            Logger::log(LogLevel::Error, "Buffer was too small for website");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Buffer was too small for website");
            return ESP_FAIL;;
        }
        
    } else if (S_ISREG(tmp_stat.st_mode)) {
        Logger::log(LogLevel::Info, "Path is a file %s %s", selected_path.data(), request_uri.data());
        int fd = open(selected_path.data(), O_RDONLY, 0);

        DoFinally closeOp( [&fd]() {
            close(fd);
        });

        if (fd == -1) {
            Logger::log(LogLevel::Warning, "Failed to open file : %s", filepath.data());
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Failed to read existing file");
            return ESP_FAIL;
        }

        setContentTypeFromFile(req, filepath.data());

        ssize_t read_bytes;
        // TODO: make configurable
        auto max_chunk_size = std::min<size_t>(buffer->size(), 4096);
        do {
            /* Read file in chunks into the scratch buffer */
            read_bytes = read(fd, buffer->data(), max_chunk_size);
            if (read_bytes == -1) {
                Logger::log(LogLevel::Warning, "Failed to read file : %s", filepath.data());
            } else if (read_bytes > 0) {
                /* Send the buffer contents as HTTP response chunk */
                esp_err_t last_result = httpd_resp_send_chunk(req, buffer->data(), read_bytes);

                if (last_result != ESP_OK) {
                    Logger::log(LogLevel::Warning, "File sending failed! %s", filepath.data());
                    /* Abort sending file */
                    httpd_resp_send_chunk(req, nullptr, 0);
                    /* Respond with 500 Internal Server Error */
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                        "Failed to send file");
                    return ESP_FAIL;

                }
            }
        } while (read_bytes > 0);
        Logger::log(LogLevel::Info, "File sending complete");
        /* Respond with an empty chunk to signal HTTP response completion */
        httpd_resp_send_chunk(req, nullptr, 0);
    }
    return ESP_OK;
}
