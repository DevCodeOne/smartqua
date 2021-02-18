#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "smartqua_config.h"
#include "actions/action_types.h"
#include "drivers/device_types.h"
#include "drivers/devices.h"
#include "utils/utils.h"
#include "storage/store.h"

json_action_result get_devices_action(std::optional<unsigned int> index, char *buf, size_t buf_len);
json_action_result add_device_action(std::optional<unsigned int> index, char *buf, size_t buf_len);
json_action_result remove_device_action(unsigned int index, char *buf, size_t buf_len);
json_action_result set_device_action(unsigned int index, char *buf, size_t buf_len);

enum struct device_collection_operation {
    ok, collection_full, index_invalid, failed
};

struct add_device { 
    std::optional<unsigned int> index = std::nullopt;
    const char description[name_length];
    const char *driver_name;
    const char *json_input;
    size_t json_len;

    struct {
        device_collection_operation collection_result = device_collection_operation::failed;
        std::optional<unsigned int> result_index = std::nullopt;
    } result;
};

struct remove_single_device {
    size_t index = std::numeric_limits<size_t>::max();
    struct {
        device_collection_operation collection_result = device_collection_operation::failed;
    } result;
};

struct read_from_device {
    size_t index = std::numeric_limits<size_t>::max();
    device_values read_value;
    
    struct {
        device_operation_result op_result = device_operation_result::failure;
        device_collection_operation collection_result = device_collection_operation::failed;
    } result;
 };

struct write_to_device { 
    size_t index = std::numeric_limits<size_t>::max();
    device_values write_value;
    
    struct {
        device_operation_result op_result = device_operation_result::failure;
        device_collection_operation collection_result = device_collection_operation::failed;
    } result;
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
    static inline constexpr size_t num_devices = max_num_devices;

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
        event.result.collection_result = device_collection_operation::failed;
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
        event.result.collection_result = device_collection_operation::collection_full;
    }

    if (*event.index < num_devices && !data.initialized[*event.index]) {
        devices[*event.index] = create_device<DeviceDrivers ...>(event.driver_name, 
            std::string_view(event.json_input, event.json_len), data.data[*event.index]);
        data.initialized[*event.index] = devices[*event.index].has_value();

        if (data.initialized[*event.index]) {
            event.result.collection_result = device_collection_operation::ok;
            event.result.result_index = *event.index;
        } else {
            event.result.collection_result = device_collection_operation::failed;
        }
    } else {
        event.result.collection_result = device_collection_operation::index_invalid;
        ESP_LOGI("Device_Settings", "Index is not valid");
    }
    return data;
}

template<typename ... DeviceDrivers>
auto device_settings<DeviceDrivers ...>::dispatch(remove_single_device &event) -> filter_return_type<remove_single_device> {
    if (event.index < num_devices && data.initialized[event.index]) {
        devices[event.index] = std::nullopt;
        data.initialized[event.index] = false;
        event.result.collection_result = device_collection_operation::ok;
    } else {
        event.result.collection_result = device_collection_operation::failed;
    }

    return data;
}

template<typename ... DeviceDrivers>
auto device_settings<DeviceDrivers ...>::dispatch(write_to_device &event) -> filter_return_type<write_to_device> {
    if (event.index < num_devices && data.initialized[event.index] && devices[event.index].has_value()) {
        ESP_LOGI("Device_Settings", "Writing to device ...");
        event.result.op_result = devices[event.index]->write_value(event.write_value);
        event.result.collection_result = device_collection_operation::ok;
    } else {
        ESP_LOGI("Device_Settings", "Device not valid");
        event.result.collection_result = device_collection_operation::index_invalid;
    }
    return data;
}

template<typename ... DeviceDrivers>
void device_settings<DeviceDrivers ...>::dispatch(read_from_device &event) const {
    if (event.index < num_devices && data.initialized[event.index] && devices[event.index].has_value()) {
        ESP_LOGI("Device_Settings", "Reading from device ...");
        event.result.op_result = devices[event.index]->read_value(event.read_value);
        event.result.collection_result = device_collection_operation::ok;
    } else {
        ESP_LOGI("Device_Settings", "Device not valid");
        event.result.collection_result = device_collection_operation::index_invalid;
    }
}