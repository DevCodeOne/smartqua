#pragma once

#include <array>
#include <charconv>
#include <type_traits>

#include "esp_http_client.h"
#include "esp_log.h"

#include "utils/rest_client.h"
#include "utils/do_finally.h"
#include "network/network_info.h"

// TODO: add setting to close the connection for storage, that's rarely updated
template<typename SettingType, RestDataType StorageType>
class RestStorage {
    public:
        template<bool isDefaultConstruct = std::is_default_constructible_v<SettingType>,
                typename std::enable_if_t<isDefaultConstruct, int> = 0>
        RestStorage() {
            isValid = SettingType::generateRestTarget(restTarget);

            if (!isValid) {
                ESP_LOGW("RestStorage", "Rest target is not valid");
                return;
            }

            clientConfig.url = restTarget.data();

            ESP_LOGI("RestStorage", "Created client with target %s", clientConfig.url);
        }

        // If the pathGenerator needs some kind of initialization
        template<typename ... Args>
        RestStorage(Args &&... args) : pathGenerator(std::forward<Args>(args) ...) {
            // TODO: Consider only using temporary instance, instead of initiation a member value
            isValid = pathGenerator.generateRestTarget(restTarget);

            if (!isValid) {
                ESP_LOGW("RestStorage", "Rest target is not valid");
                return;
            }

            clientConfig.url = restTarget.data();

            ESP_LOGI("RestStorage", "Created client with target %s", clientConfig.url);
        }

        RestStorage(const RestStorage &other) = delete;
        RestStorage(RestStorage &&other) 
            : client(other.client), isValid(other.isValid), restTarget(other.restTarget), pathGenerator(other.pathGenerator) {
            other.clientConfig.url = nullptr;
            other.client = nullptr;
        }

        RestStorage &operator=(const RestStorage &other) = delete;
        RestStorage &operator=(RestStorage &&other) {
            using std::swap;

            swap(clientConfig, other.clientConfig);
            swap(client, other.client);
            swap(pathGenerator, other.pathGenerator);
            swap(isValid, other.isValid);

            other.client = nullptr;

            return *this;
        }

        ~RestStorage() {
        }

        bool retrieveData(char *dst, size_t len) {
            if (!NetworkInfo::canUseNetwork()) {
                ESP_LOGW("RestStorage", "RestStorage is not valid (%d)", isValid);
                return false;
            }

            client = esp_http_client_init(&clientConfig);

            if (client == nullptr) {
                ESP_LOGW("RestStorage", "Couldn't create client with target %s", clientConfig.url);
                isValid = false;
                return false;
            }

            DoFinally cleanup([this]() {
                esp_http_client_close(this->client);
                esp_http_client_cleanup(this->client);
                this->client = nullptr;
            });

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
                esp_http_client_close(client);
                ESP_LOGW("RestStorage", "Didn't get 200 Ok");
                return false; 
            }

            const auto read = esp_http_client_read(client, dst, len);

            if (read == -1) { 
                ESP_LOGW("RestStorage", "Read returned -1");
                return false; 
            }

            if (error != ESP_OK) { 
                ESP_LOGW("RestStorage", "Connection close returned -1, ignoring");
            }

            ESP_LOGI("RestStorage", "Read %d bytes from %s", (int) read, clientConfig.url);

            return true;
        }

        template<esp_http_client_method_t Method = HTTP_METHOD_PUT>
        bool writeData(const char *src, size_t len) {
            if (!isValid || !NetworkInfo::canUseNetwork()) {
                ESP_LOGW("RestStorage", "RestStorage is not valid (%d)", isValid);
                return false;
            }

            clientConfig.url = restTarget.data();
            client = esp_http_client_init(&clientConfig);

            if (client == nullptr) {
                ESP_LOGW("RestStorage", "Couldn't create client with target %s", clientConfig.url);
                isValid = false;
                return false;
            }

            DoFinally cleanup([this]() {
                esp_http_client_close(this->client);
                esp_http_client_cleanup(this->client);
            });

            auto error = esp_http_client_set_method(client, Method);

            if (error != ESP_OK) { 
                ESP_LOGW("RestStorage", "Couldn't set method");
                return false; 
            }

            error = esp_http_client_set_header(client, "Content-Type", DataTypeToContentType<StorageType>::ContentType);

            if (error != ESP_OK) { 
                ESP_LOGW("RestStorage", "Couldn't set headers");
                return false; 
            }

            error = esp_http_client_open(client, len + 1);

            if (error != ESP_OK) { 
                ESP_LOGW("RestStorage", "Couldn't open stream to write");
                error = esp_http_client_close(client);
                return false; 
            }

            auto written = esp_http_client_write(client, src, len);

            if (written == -1) { 
                ESP_LOGW("RestStorage", "Couldn't write to stream");
                error = esp_http_client_close(client);
                return false; 
            }

            if (error != ESP_OK) { 
                ESP_LOGW("RestStorage", "Couldn't write to stream");
                error = esp_http_client_close(client);
                return false; 
            }
            
            error = esp_http_client_close(client);

            if (error != ESP_OK) { 
                // ESP_LOGW("RestStorage", "Fail on http close, ignoring this error");
            }

            // ESP_LOGI("RestStorage", "Wrote %d bytes to %s", (int) len, clientConfig.url);

            return true;
        }

        operator bool() const {
            return isValid;
        }

    private:
        esp_http_client_handle_t client = nullptr;
        esp_http_client_config_t clientConfig{};
        bool isValid;
        std::array<char, 128> restTarget{};
        SettingType pathGenerator;
};