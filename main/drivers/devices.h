#pragma once

#include <array>
#include <optional>
#include <variant>
#include <cstring>
#include <string_view>

#include "frozen.h"
#include "esp_log.h"

#include "smartqua_config.h"
#include "device_types.h"
#include "utils/utils.h"

// TODO: setup drivers
// one driver for all devices on that bus ?
// give driver control over specific gpios
// also add gpio ownership struct 
// DeviceDriver built as monostate, with minimal device specific infos e.g. singular id for device on a bus
// 1. setup
// 2. operate
// 3. deinitialize

// TODO: maybe split into categories
// TODO: custom data types
// TODO: maybe use fixed size decimal numbers
// TODO: where to put the list of device drivers ?
template<typename ... DeviceDrivers>
class device final {
public:
    template<typename DriverType>
    device(DriverType driver);
    device(const device &) = default;
    device(device &&) = default;
    ~device() = default;

    device &operator=(const device &other) = default;
    device &operator=(device &&other) = default;

    device_operation_result write_value(const device_values &value);
    // To calibrate something or execute actions
    device_operation_result write_options(const std::string_view &jsonSettingValue);
    device_operation_result update_runtime_data();
    device_operation_result read_value(device_values &value) const;
    device_operation_result get_info(char *output_buffer, size_t output_buffer_len) const;

private:
    std::variant<DeviceDrivers ...> m_driver = nullptr;
};

// Read data from data_in and initialize driver with data_in
// the correct driver will be found by the device_name
// device_conf_out will contain the necessary data for the driver to be initialized from storage
template<typename ... DeviceDrivers>
std::optional<device<DeviceDrivers ...>> create_device(std::string_view driver_name, std::string_view input, device_config &device_conf_out) {
    std::optional<device<DeviceDrivers ...>> found_device_driver = std::nullopt; 
    ESP_LOGI("Device_Driver", "Searching driver %.*s", driver_name.length(), driver_name.data());

    constexpr_for_index<sizeof...(DeviceDrivers) - 1>::doCall(
        [input, driver_name, &device_conf_out, &found_device_driver](auto current_index) constexpr {
            using driver_type = std::tuple_element_t<decltype(current_index)::value, std::tuple<DeviceDrivers ...>>;

            // Not the driver we are looking for
            if (driver_name != driver_type::name) {
                return;
            }

             ESP_LOGI("Device_Driver", "Found driver %.*s", driver_name.length(), driver_name.data());
            device_conf_out.device_driver_name = driver_type::name;
            auto result = driver_type::create_driver(input, device_conf_out);
            
            // Driver creation wasn't successfull
            if (!result.has_value()) {
                return;
            }

            found_device_driver = std::move(*result);
        });

    return found_device_driver;
}

template<typename ... DeviceDrivers>
std::optional<device<DeviceDrivers ...>> create_device(const device_config *device_conf) {
    std::optional<device<DeviceDrivers ...>> found_device_driver = std::nullopt; 

    constexpr_for_index<sizeof...(DeviceDrivers) - 1>::doCall([&found_device_driver, &device_conf](auto current_index){
        using driver_type = std::tuple_element_t<current_index, std::tuple<DeviceDrivers ...>>;

        if (std::strncmp(device_conf->device_driver_name.data(), driver_type::name, name_length) == 0) {
            ESP_LOGI("Device_Driver", "Found driver %s", device_conf->device_driver_name.data());
            auto result = driver_type::create_driver(device_conf);

            if (!result.has_value()) {
                ESP_LOGI("Device_Driver", "Failed to create device with driver %s", device_conf->device_driver_name.data());
                return;
            }

            ESP_LOGI("Device_Driver", "Created device with driver %s", device_conf->device_driver_name.data());
            found_device_driver = std::move(*result);
        }
    });

    return found_device_driver;
}

template<typename ... DeviceDrivers>
template<typename DriverType>
device<DeviceDrivers ...>::device(DriverType driver) : m_driver(std::move(driver)) {}

template<typename ... DeviceDrivers>
device_operation_result device<DeviceDrivers ...>::write_value(const device_values &value) {
    return std::visit(
        [&value](auto &current_driver) { 
            return current_driver.write_value(value); 
        }, m_driver);
}

template<typename ... DeviceDrivers>
device_operation_result device<DeviceDrivers ...>::read_value(device_values &value) const {
    return std::visit(
        [&value](const auto &current_driver) { 
            ESP_LOGI("Device_Driver", "Delegating to driver");
            return current_driver.read_value(value); 
        }, m_driver);
}

template<typename ... DeviceDrivers>
device_operation_result device<DeviceDrivers ...>::get_info(char *output_buffer, size_t output_buffer_len) const {
    return std::visit(
        [output_buffer, output_buffer_len](const auto &current_driver) { 
            ESP_LOGI("Device_Driver", "Delegating to driver");
            return current_driver.get_info(output_buffer, output_buffer_len); 
        }, m_driver);
}

template<typename ... DeviceDrivers>
device_operation_result device<DeviceDrivers ...>::write_options(const std::string_view &jsonSettingValue) {
    return std::visit(
        [&jsonSettingValue](const auto &current_driver) { 
            ESP_LOGI("Device_Driver", "Delegating to driver");
            return current_driver.write_options(jsonSettingValue.data(), jsonSettingValue.length()); 
        }, m_driver);
}

template<typename ... DeviceDrivers>
device_operation_result device<DeviceDrivers ...>::update_runtime_data() {
    return std::visit(
        [](auto &current_driver) { 
            ESP_LOGI("Device_Driver", "Delegating to driver");
            return current_driver.update_runtime_data(); 
        }, m_driver);
}