#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "esp_http_server.h"

#include "smartqua_config.h"
#include "drivers/device_types.h"
#include "drivers/devices.h"
#include "utils/utils.h"
#include "storage/store.h"

esp_err_t get_devices(httpd_req *req);
esp_err_t post_device(httpd_req *req);
esp_err_t set_device(httpd_req *req);
esp_err_t remove_device(httpd_req *req);

enum struct device_collection_operation {
    ok, collection_full, index_invalid, failed
};

struct add_device { 
    std::optional<unsigned int> index = std::nullopt;
    const char description[name_length];
    const char *driver_name;
    const char *json_input;
    size_t json_len;
    device_collection_operation op_result;
};

struct remove_single_device {
    size_t index = std::numeric_limits<size_t>::max();
    device_collection_operation op_result;
};

struct read_from_device {
    size_t index = std::numeric_limits<size_t>::max();
    device_operation_result op_result;
    device_values read_value;
 };

struct write_to_device { 
    size_t index = std::numeric_limits<size_t>::max();
    device_operation_result op_result;
    device_values write_value;
};

template<size_t N> 
struct device_setting_trivial {
    static inline constexpr char name[] = "device_config";

    std::array<device_config, N> data;
    std::array<std::array<char, name_length>, N> description;
    std::array<bool, N> initialized;
};

template<typename ... DeviceDrivers>
class device_settings final {
public:
    static inline constexpr size_t num_devices = 15;

    device_settings() = default;
    ~device_settings() = default;

    using trivial_representation = device_setting_trivial<num_devices>;
    template<typename T>
    using filter_return_type = std::conditional_t<!
        all_unique_v<T,
        add_device, remove_single_device, write_to_device>, trivial_representation, ignored_event>;

    device_settings &operator=(trivial_representation new_value);
    
    // Ignore other read and write events
    template<typename T>
    filter_return_type<T> dispatch(T &event) {}

    template<typename T>
    void dispatch(T &event) const {}

    filter_return_type<add_device> dispatch(add_device &event);

    filter_return_type<remove_single_device> dispatch(remove_single_device &event);

    filter_return_type<write_to_device> dispatch(write_to_device &event);

    // TODO: maybe different return type ?
    void dispatch(read_from_device &event) const;
private:
    trivial_representation data;
    std::array<std::optional<device<DeviceDrivers ...>>, num_devices> devices;
};

// TODO: implement
template<typename ... DeviceDrivers>
device_settings<DeviceDrivers ...> &device_settings<DeviceDrivers ...>::operator=(trivial_representation new_value) {
    data = new_value;

    for (size_t i = 0; i < data.initialized.size(); ++i) {
        if (data.initialized[i]) {
            devices[i] = create_device<DeviceDrivers ...>(&data.data[i]);
        }
    }

    return *this;
}

template<typename ... DeviceDrivers>
auto device_settings<DeviceDrivers ...>::dispatch(add_device &event) -> filter_return_type<add_device> {
    ESP_LOGI("Device_Settings", "Trying to add new device");
    if (event.driver_name == nullptr) {
        ESP_LOGI("Device_Settings", "No driver_name provided");
    }

    if (!event.index.has_value()) {
        for (unsigned int i = 0; i < data.initialized.size(); ++i) {
            if (!data.initialized[i]) {
                event.index = i;
                break;
            }
        }
    }

    if (!event.index.has_value()) {
        ESP_LOGI("Device_Settings", "No free index found");
    }

    if (*event.index < num_devices && !data.initialized[*event.index]) {
        devices[*event.index] = create_device<DeviceDrivers ...>(event.driver_name, 
            std::string_view(event.json_input, event.json_len), data.data[*event.index]);
        data.initialized[*event.index] = devices[*event.index].has_value();
    } else {
        ESP_LOGI("Device_Settings", "Index is not valid");
    }
    return data;
}

template<typename ... DeviceDrivers>
auto device_settings<DeviceDrivers ...>::dispatch(remove_single_device &event) -> filter_return_type<remove_single_device> {
    if (event.index < num_devices && data.initialized[event.index]) {
        devices[event.index] = std::nullopt;
        data.initialized[event.index] = false;
    }

    return data;
}

template<typename ... DeviceDrivers>
auto device_settings<DeviceDrivers ...>::dispatch(write_to_device &event) -> filter_return_type<write_to_device> {
    if (event.index < num_devices && data.initialized[event.index] && devices[event.index].has_value()) {
        ESP_LOGI("Device_Settings", "Writing to device ...");
        event.op_result = devices[event.index]->write_value(event.write_value);
    } else {
        ESP_LOGI("Device_Settings", "Device not valid");
    }
    return data;
}

template<typename ... DeviceDrivers>
void device_settings<DeviceDrivers ...>::dispatch(read_from_device &event) const {
    if (event.index < num_devices && data.initialized[event.index] && devices[event.index].has_value()) {
        ESP_LOGI("Device_Settings", "Reading from device ...");
        event.op_result = devices[event.index]->read_value(event.read_value);
    } else {
        ESP_LOGI("Device_Settings", "Device not valid");
    }
}