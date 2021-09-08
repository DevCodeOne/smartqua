#include "ds18x20_driver.h"

#include <algorithm>
#include <cstring>

#include "frozen.h"
#include "ds18x20.h"
#include "esp_log.h"

#include "smartqua_config.h"

std::optional<ds18x20_driver> ds18x20_driver::create_driver(const device_config *config) {
    auto driver_data = reinterpret_cast<const ds18x20_driver_data *>(config->device_config.data());
    auto pin = device_resource::get_gpio_resource(driver_data->gpio, gpio_purpose::bus);

    if (!pin) {
        ESP_LOGI("ds18x20_driver", "GPIO pin couldn't be reserved");
        return std::nullopt;
    }

    /*
    std::array<ds18x20_addr_t, max_num_devices> sensor_addresses;
    auto detected_sensors = ds18x20_scan_devices(static_cast<gpio_num_t>(driver_data->gpio), sensor_addresses.data(), max_num_devices);

    if (detected_sensors < 1) {
        ESP_LOGI("ds18x20_driver", "Didn't find any devices on gpio : %u", driver_data->gpio);
        return std::nullopt;
    }

    auto result = std::find(sensor_addresses.cbegin(), sensor_addresses.cend(), driver_data->addr);

    if (result == sensor_addresses.cend()) {
        ESP_LOGI("ds18x20_driver", "Didn't this specific device");
        return std::nullopt;
    }*/

    if (!add_address(driver_data->addr)) {
        ESP_LOGI("ds18x20_driver", "The device is already in use");
        return std::nullopt;
    }
    
    return std::optional<ds18x20_driver>(ds18x20_driver(config, pin));
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

    auto pin = device_resource::get_gpio_resource(static_cast<gpio_num_t>(gpio_num), gpio_purpose::bus);

    if (!pin) {
        ESP_LOGI("ds18x20_driver", "GPIO pin couldn't be reserved");
        return std::nullopt;
    }

    auto detected_sensors = ds18x20_scan_devices(static_cast<gpio_num_t>(gpio_num), sensor_addresses.data(), max_num_devices);

    if (detected_sensors < 1) {
        ESP_LOGI("ds18x20_driver", "Didn't find any devices on port : %d", gpio_num);
        return std::nullopt;
    }

    // Skip already found addresses and use only the new ones
    std::optional<unsigned int> index_to_add = std::nullopt;

    for (unsigned int i = 0; i < detected_sensors; ++i) {
        if (add_address(sensor_addresses[i])) {
            index_to_add = i;
        }
    }

    if (!index_to_add.has_value()) {
        return std::nullopt;
    }

    ESP_LOGI("ds18x20_driver", "Found devices on gpio_num : %d @ address : %llu", gpio_num, sensor_addresses[*index_to_add]);

    ds18x20_driver_data data { static_cast<gpio_num_t>(gpio_num), sensor_addresses[*index_to_add] };
    std::memcpy(reinterpret_cast<void *>(&device_conf_out.device_config), reinterpret_cast<void *>(&data), sizeof(ds18x20_driver_data));
    device_conf_out.device_driver_name =  ds18x20_driver::name;

    return std::make_optional<ds18x20_driver>(ds18x20_driver(&device_conf_out, pin));
}

ds18x20_driver::ds18x20_driver(const device_config *conf, std::shared_ptr<gpio_resource> pin) : m_conf(conf), m_pin(pin) { }

ds18x20_driver::ds18x20_driver(ds18x20_driver &&other) : m_conf(other.m_conf), m_pin(other.m_pin) {
    other.m_conf = nullptr;
    other.m_pin = nullptr;
 }

 ds18x20_driver &ds18x20_driver::operator=(ds18x20_driver &&other) {
    using std::swap;

    swap(m_conf, other.m_conf);
    swap(m_pin, other.m_pin);

    return *this;
}

ds18x20_driver::~ds18x20_driver() { 
    if (m_conf) {
        remove_address(reinterpret_cast<ds18x20_driver_data *>(m_conf->device_config.data())->addr);
    }
}

device_operation_result ds18x20_driver::write_value(const device_values &value) { 
    return device_operation_result::not_supported;
}

device_operation_result ds18x20_driver::read_value(device_values &value) const {
    const auto *config  = reinterpret_cast<const ds18x20_driver_data *>(&m_conf->device_config);
    float temperature = 0.0f;
    ESP_LOGI("ds18x20_driver", "Reading from gpio_num : %d @ address : %u%u", static_cast<int>(config->gpio),
        static_cast<uint32_t>(config->addr >> 32),
        static_cast<uint32_t>(config->addr & ((1ull << 32) - 1)));

    auto result = ds18x20_measure_and_read(config->gpio, config->addr, &temperature);

    ESP_LOGI("ds18x20_driver", "Read temperature : %d", static_cast<int>(temperature * 1000));

    if (result != ESP_OK) {
        return device_operation_result::failure;
    }

    value.temperature = temperature;
    return device_operation_result::ok;
}

// TODO: implement both
device_operation_result ds18x20_driver::get_info(char *output_buffer, size_t output_buffer_len) const {
    json_out out = JSON_OUT_BUF(output_buffer, output_buffer_len);
    json_printf(&out, "{}");
    return device_operation_result::ok;
}

device_operation_result ds18x20_driver::write_device_options(const char *json_input, size_t input_len) {
    return device_operation_result::ok;
}

bool ds18x20_driver::add_address(ds18x20_addr_t address) {
    std::unique_lock instance_guard{_instance_mutex};

    bool adress_already_exists = std::any_of(ds18x20_driver::_device_addresses.cbegin(), ds18x20_driver::_device_addresses.cend(), 
        [&address](const auto &already_found_address) {
            return already_found_address.has_value() && *already_found_address == address;
        });

    if (adress_already_exists) {
        ESP_LOGI("ds18x20_driver", "No new address, don't add this address");
        return false;
    }

    auto first_empty_slot = std::find(ds18x20_driver::_device_addresses.begin(), ds18x20_driver::_device_addresses.end(), std::nullopt);

    // No free addresses
    if (first_empty_slot == ds18x20_driver::_device_addresses.cend()) {
        return false;
    }

    *first_empty_slot = address;

    return true;
}

bool ds18x20_driver::remove_address(ds18x20_addr_t address) {
    std::unique_lock instance_guard{_instance_mutex};

    auto found_address = std::find_if(ds18x20_driver::_device_addresses.begin(), ds18x20_driver::_device_addresses.end(), 
        [&address](auto &current_address) {
            return current_address.has_value() && *current_address == address;
        });

    if (found_address == ds18x20_driver::_device_addresses.end()) {
        return false;
    }

    *found_address = std::nullopt;

    return true;
}