#pragma once

#include "esp_http_client.h"

enum struct InitPolicy {
    Directly, Lazy
};

enum struct ResourcePolicy {
    OneTimeOnly, Reuse
};

enum struct RestDataType {
    Binary, Text, Json
};

template<RestDataType DataType>
struct DataTypeToContentType;

template<>
struct DataTypeToContentType<RestDataType::Binary> { static inline constexpr char ContentType [] = "application/octet-stream"; };

template<>
struct DataTypeToContentType<RestDataType::Text>  { static inline constexpr char ContentType [] = "plain/text"; };

template<>
struct DataTypeToContentType<RestDataType::Json>  { static inline constexpr char ContentType [] = "application/json"; };

template<typename T, ResourcePolicy ResPolicy = ResourcePolicy::OneTimeOnly, InitPolicy InPolicy = InitPolicy::Lazy>
class RestClient {
    public:
        RestClient();
        RestClient(RestClient &other) = delete;
        RestClient(RestClient &&other);
        ~RestClient();

        RestClient &operator=(const RestClient &other) = delete;
        RestClient &operator=(const RestClient &&other) = default;

        bool writeData(RestDataType dataType, char *src, size_t len) const;
        bool readData(RestDataType dataType, char *dst, size_t len) const;

        bool setUrl(const char *url);

        bool isValid() const;
    private:

        bool initClient();
        bool deInitClient();

        bool isInitialized = false;
        esp_http_client_config_t clientConfig{};
};

template<typename T, ResourcePolicy ResPolicy, InitPolicy InPolicy>
RestClient<T, ResPolicy, InPolicy>::RestClient() {
    if constexpr (InPolicy == InitPolicy::Directly) {
        initClient();
    }
}

template<typename T, ResourcePolicy ResPolicy, InitPolicy InPolicy>
RestClient<T, ResPolicy, InPolicy>::~RestClient() {
    deInitClient();
}

template<typename T, ResourcePolicy ResPolicy, InitPolicy InPolicy>
bool RestClient<T, ResPolicy, InPolicy>::initClient() {
    return true;
}

template<typename T, ResourcePolicy ResPolicy, InitPolicy InPolicy>
bool RestClient<T, ResPolicy, InPolicy>::deInitClient() {
    return true;
}