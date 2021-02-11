#pragma once

#include <array>
#include <optional>
#include <variant>

#include "smartqua_config.h"
#include "device_types.h"

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
    ~device() = default;

    device_read_result write_value(const device_values &value);
    device_write_result read_value(device_values &value);
private:
    std::variant<DeviceDrivers ...> m_driver = nullptr;
};

template<typename ... DeviceDrivers>
std::optional<device<DeviceDrivers ...>> create_device(const device_config *device_conf) {
    // TODO: search for index for type wich has a name constant that matches the one in the device_conf

    using device_type = device;

    auto result = device_type::create_driver(device_conf);

    if (!result) {
        return std::nullopt;
    }

    return std::make_optional<device<DeviceDrivers ...>>(std::move(*result));
}

template<typename ... DeviceDrivers>
template<typename DriverType>
device<DeviceDriver ...>::device(DriverType driver) : m_driver(std::move(driver)) {}

template<typename ... DeviceDrivers>
device_read_result device<DeviceDriver ...>::write_value(const device_values &value) {
    return std::visit([&value](auto &current_driver) { return current_driver.write_value(value); }, m_driver);
}

template<typename ... DeviceDrivers>
device_read_result device<DeviceDriver ...>::write_value(device_values &value) {
    return std::visit([&value](auto &current_driver) { return current_driver.write_value(value); }, m_driver);
}