#include "ds18x20_driver.h"

#include <algorithm>
#include <cstring>

#include "frozen.h"
#include "ds18x20.h"
#include "esp_log.h"

#include "smartqua_config.h"

// TODO: check if device is valid and add device addresses to address
std::optional<ds18x20_driver> ds18x20_driver::create_driver(const device_config *config) {
    return std::optional<ds18x20_driver>(ds18x20_driver(config));
}

std::optional<ds18x20_driver> ds18x20_driver::create_driver(const std::string_view input, device_config &device_conf_out) {
    std::array<ds18x20_addr_t, max_num_devices> sensor_addresses;
    int gpio_num = -1;
    json_scanf(input.data(), input.size(), R"({ gpio_num : %d})", &gpio_num);
    ESP_LOGI("ds18x20_driver", "gpio_num : %d", gpio_num);

    if (gpio_num == -1) {
        ESP_LOGI("ds18x20_driver", "Invalid gpio num : %d", gpio_num);
        return std::nullopt;
    }

    auto detected_sensors = ds18x20_scan_devices(static_cast<gpio_num_t>(gpio_num), sensor_addresses.data(), max_num_devices);

    if (detected_sensors < 1) {
        ESP_LOGI("ds18x20_driver", "Didn't find any devices on port : %d", gpio_num);
        return std::nullopt;
    }

    // Skip already found addresses and use only the new ones

    // Mutex block
    {
        std::unique_lock instance_guard{_instance_mutex};

        auto new_address = std::find_if(sensor_addresses.cbegin(), sensor_addresses.cend(), [](auto current_address) {
            return std::none_of(ds18x20_driver::_devices_addresses.cbegin(), ds18x20_driver::_devices_addresses.cend(), 
            [&current_address](auto &already_found_address) {
                return already_found_address.has_value() && *already_found_address == current_address;
            });
        });

        if (new_address == sensor_addresses.cend()) {
            return std::nullopt;
        }

        auto first_empty_slot = std::find_if(ds18x20_driver::_devices_addresses.begin(), ds18x20_driver::_devices_addresses.end(),
            [](auto &current_entry) {
                return !current_entry.has_value();
            });

        if (first_empty_slot == ds18x20_driver::_devices_addresses.cend()) {
            return std::nullopt;
        }

        ESP_LOGI("ds18x20_driver", "Found devices on gpio_num : %d @ address : %lld", gpio_num, *new_address);

        ds18x20_driver_data data { static_cast<gpio_num_t>(gpio_num), *new_address };
        std::memcpy(reinterpret_cast<void *>(&device_conf_out.device_config), reinterpret_cast<void *>(&data), sizeof(ds18x20_driver_data));
        std::strncpy(device_conf_out.device_driver_name.data(), ds18x20_driver::name, name_length);
        *first_empty_slot = *new_address;
    }

    return std::make_optional<ds18x20_driver>(ds18x20_driver(&device_conf_out));
}

ds18x20_driver::ds18x20_driver(const device_config *conf) : m_conf(conf) { }

device_write_result ds18x20_driver::write_value(const device_values &value) { 
    return device_write_result::not_supported;
}

device_read_result ds18x20_driver::read_value(device_values &value) const {
    const auto *config  = reinterpret_cast<const ds18x20_driver_data *>(&m_conf->device_config);
    float temperature = 0.0f;
    ESP_LOGI("ds18x20_driver", "Reading from gpio_num : %d @ address : %u%u", static_cast<int>(config->gpio),
        static_cast<uint32_t>(config->addr >> 32),
        static_cast<uint32_t>(config->addr & ((1ull << 32) - 1)));

    auto result = ds18x20_measure_and_read(config->gpio, config->addr, &temperature);

    ESP_LOGI("ds18x20_driver", "Read temperature : %d", static_cast<int>(temperature * 1000));

    if (result != ESP_OK) {
        return device_read_result::failure;
    }

    value.temperature = temperature;
    return device_read_result::ok;
}