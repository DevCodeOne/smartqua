#pragma once

#include <array>
#include <charconv>

#include "esp_http_client.h"
#include "esp_log.h"

#include "utils/rest_client.h"

// TODO: add setting to close the connection for storage, that's rarely updated
template<typename SettingType, RestDataType StorageType>
class RestStorage {
    public:
        RestStorage() {
            isValid = SettingType::generateRestTarget(restTarget);

            if (!isValid) {
                ESP_LOGW("RestStorage", "Rest target is not valid");
                return;
            }

            clientConfig.url = restTarget.data();
            client = esp_http_client_init(&clientConfig);

            if (client == nullptr) {
                ESP_LOGW("RestStorage", "Couldn't create client with target %s", clientConfig.url);
                isValid = false;
                return;
            }

            ESP_LOGI("RestStorage", "Created client with target %s", clientConfig.url);
        }

        RestStorage(const RestStorage &other) = delete;
        RestStorage(RestStorage &&other) : client(other.client), restTarget(other.restTarget) {
            clientConfig.url = nullptr;
            other.client = nullptr;
        }

        RestStorage &operator=(const RestStorage &other) = delete;
        RestStorage &operator=(RestStorage &&other) {
            using std::swap;

            swap(clientConfig, other.clientConfig);
            swap(client, other.client);

            clientConfig.url = restTarget.data();

            return *this;
        }

        ~RestStorage() {
            if (client != nullptr) {
                esp_http_client_cleanup(client);
            }
        }

        bool retrieveData(char *dst, size_t len) {
            if (!isValid) {
                ESP_LOGW("RestStorage", "RestStorage is not valid");
                return false;
            }

            ESP_LOGI("RestStorage", "Try to read %d bytes from %s", (int) len, clientConfig.url);

            esp_http_client_set_method(client, HTTP_METHOD_GET);
            auto error = esp_http_client_set_header(client, "Content-Type", DataTypeToContentType<StorageType>::ContentType);

            if (error != ESP_OK) { return false; }

            error = esp_http_client_open(client, 0);

            if (error != ESP_OK) { return false; }

            error = esp_http_client_write(client, nullptr, 0);

            if (error != ESP_OK) { return false; }

            const auto fetchResult = esp_http_client_fetch_headers(client);

            if (fetchResult == -1) { 
                ESP_LOGW("RestStorage", "Failed fetching headers");
                return false; 
            }

            const auto getResult = esp_http_client_get_status_code(client);

            if (getResult != 200) { 
                ESP_LOGW("RestStorage", "Didn't get 200 Ok");
                return false; 
            }

            const auto read = esp_http_client_read(client, dst, len);

            if (read == -1) { 
                ESP_LOGW("RestStorage", "Read returned -1");
                return false; 
            }

            error = esp_http_client_close(client);

            if (error != ESP_OK) { 
                ESP_LOGW("RestStorage", "Connection close returned -1, ignoring");
            }

            ESP_LOGI("RestStorage", "Read %d bytes from %s", (int) read, clientConfig.url);

            return true;
        }

        bool writeData(const char *src, size_t len) {
            if (!isValid) {
                ESP_LOGW("RestStorage", "RestStorage is not valid");
                return false;
            }

            ESP_LOGI("RestStorage", "Try to write %d bytes to %s", (int) len, clientConfig.url);

            esp_http_client_set_method(client, HTTP_METHOD_PUT);

            // std::array<char, 32> numbersDst{'\0'};
            // std::to_chars(numbersDst.data(), numbersDst.data() + numbersDst.size(), len);
            // TODO: check return value

            auto error = esp_http_client_set_header(client, "Content-Type", DataTypeToContentType<StorageType>::ContentType);

            if (error != ESP_OK) { 
                ESP_LOGW("RestStorage", "Couldn't set headers");
                return false; 
            }

            error = esp_http_client_open(client, len + 1);

            if (error != ESP_OK) { 
                ESP_LOGW("RestStorage", "Couldn't open stream to write");
                return false; 
            }

            auto written = esp_http_client_write(client, src, len);

            if (written == -1) { 
                ESP_LOGW("RestStorage", "Couldn't write to stream");
                return false; 
            }

            if (error != ESP_OK) { 
                ESP_LOGW("RestStorage", "Couldn't write to stream");
                return false; 
            }
            
            error = esp_http_client_close(client);

            if (error != ESP_OK) { 
                ESP_LOGW("RestStorage", "Fail on http close, ignoring this error");
            }

            ESP_LOGI("RestStorage", "Wrote %d bytes to %s", (int) len, clientConfig.url);

            return true;
        }

        operator bool() const {
            return isValid;
        }

    private:
        esp_http_client_handle_t client{};
        esp_http_client_config_t clientConfig{};
        bool isValid;
        std::array<char, 96> restTarget{};
};