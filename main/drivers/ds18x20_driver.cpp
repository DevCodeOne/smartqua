#include "ds18x20_driver.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

#include "frozen.h"
#include "ds18x20.h"

#include "build_config.h"

std::optional<Ds18x20Driver> Ds18x20Driver::create_driver(const DeviceConfig*config) {
    auto driver_data = reinterpret_cast<const ds18x20_driver_data *>(config->device_config.data());
    auto pin = DeviceResource::get_gpio_resource(driver_data->gpio, GpioPurpose::bus);

    if (!pin) {
        Logger::log(LogLevel::Warning, "GPIO pin couldn't be reserved");
        return std::nullopt;
    }

    /*
    std::array<ds18x20_addr_t, max_num_devices> sensor_addresses;
    auto detected_sensors = ds18x20_scan_devices(static_cast<gpio_num_t>(driver_data->gpio), sensor_addresses.data(), max_num_devices);

    if (detected_sensors < 1) {
        Logger::log(LogLevel::Warning, "Didn't find any devices on gpio : %u", driver_data->gpio);
        return std::nullopt;
    }

    auto result = std::find(sensor_addresses.cbegin(), sensor_addresses.cend(), driver_data->addr);

    if (result == sensor_addresses.cend()) {
        Logger::log(LogLevel::Warning, "Didn't this specific device");
        return std::nullopt;
    }*/

    if (!add_address(driver_data->addr)) {
        Logger::log(LogLevel::Warning, "The device is already in use");
        return std::nullopt;
    }
    
    return Ds18x20Driver(config, pin);
}

std::optional<Ds18x20Driver> Ds18x20Driver::create_driver(const std::string_view input, DeviceConfig&device_conf_out) {
    std::array<ds18x20_addr_t, max_num_devices> sensor_addresses;
    int gpio_num = -1;
    json_scanf(input.data(), input.size(), R"({ gpio_num : %d})", &gpio_num);
    Logger::log(LogLevel::Info, "gpio_num : %d", gpio_num);

    if (gpio_num == -1) {
        Logger::log(LogLevel::Warning, "Invalid gpio num : %d", gpio_num);
        return std::nullopt;
    }

    auto pin = DeviceResource::get_gpio_resource(static_cast<gpio_num_t>(gpio_num), GpioPurpose::bus);

    if (!pin) {
        Logger::log(LogLevel::Warning, "GPIO pin couldn't be reserved");
        return std::nullopt;
    }

    size_t numDevicesFound = -1;
    auto result = ds18x20_scan_devices(static_cast<gpio_num_t>(gpio_num), sensor_addresses.data(), max_num_devices, &numDevicesFound);

    if (result != ESP_OK || numDevicesFound < 1) {
        Logger::log(LogLevel::Warning, "Didn't find any devices on port : %d, result : %d, num_devices : %d", gpio_num, (int) result, (int) numDevicesFound);
        return std::nullopt;
    }

    // Skip already found addresses and use only the new ones
    std::optional<unsigned int> index_to_add = std::nullopt;

    for (unsigned int i = 0; i < numDevicesFound; ++i) {
        if (add_address(sensor_addresses[i])) {
            index_to_add = i;
        }
    }

    if (!index_to_add.has_value()) {
        Logger::log(LogLevel::Info, "Didn't find any devices attaced to gpio_num : %d", gpio_num);
        return std::nullopt;
    }

    Logger::log(LogLevel::Info, "Found devices on gpio_num : %d @ address : %llu", gpio_num, sensor_addresses[*index_to_add]);

    ds18x20_driver_data data { static_cast<gpio_num_t>(gpio_num), sensor_addresses[*index_to_add] };
    std::memcpy(reinterpret_cast<void *>(device_conf_out.device_config.data()), &data, sizeof(ds18x20_driver_data));
    device_conf_out.device_driver_name =  Ds18x20Driver::name;

    return Ds18x20Driver(&device_conf_out, pin);
}

Ds18x20Driver::Ds18x20Driver(const DeviceConfig*conf, std::shared_ptr<GpioResource> pin) : m_conf(conf), m_pin(pin) {
    mTemperatureThread = std::jthread(&Ds18x20Driver::updateTempThread, this);
}

Ds18x20Driver::Ds18x20Driver(Ds18x20Driver &&other) : m_conf(other.m_conf), m_pin(other.m_pin) {

    other.mTemperatureThread.request_stop();
    if (other.mTemperatureThread.joinable()) {
        other.mTemperatureThread.join();
    }

    other.m_conf = nullptr;
    other.m_pin = nullptr;
    
    mTemperatureThread = std::jthread(&Ds18x20Driver::updateTempThread, this);
 }

 Ds18x20Driver &Ds18x20Driver::operator=(Ds18x20Driver &&other) {
    using std::swap;

    Logger::log(LogLevel::Info, "Waiting for other thread to join move op");
    other.mTemperatureThread.request_stop();
    if (other.mTemperatureThread.joinable()) {
        other.mTemperatureThread.join();
    }
    Logger::log(LogLevel::Info, "Done");

    swap(m_conf, other.m_conf);
    swap(m_pin, other.m_pin);

    mTemperatureThread = std::jthread(&Ds18x20Driver::updateTempThread, this);

    return *this;
}

Ds18x20Driver::~Ds18x20Driver() { 
    if (m_conf == nullptr) {
        return;
    }

    Logger::log(LogLevel::Info, "Deleting instance of stepperdosingpumpdriver");
    if (mTemperatureThread.joinable()) {
        mTemperatureThread.request_stop();
        mTemperatureThread.join();
    }
    remove_address(reinterpret_cast<ds18x20_driver_data *>(m_conf->device_config.data())->addr);
}

DeviceOperationResult Ds18x20Driver::write_value(std::string_view what, const DeviceValues &value) {
    return DeviceOperationResult::not_supported;
}

// TODO: read sample value and sample values in update_runtime_data, or even in a seperate thread
DeviceOperationResult Ds18x20Driver::read_value(std::string_view what, DeviceValues &value) const {
    value.temperature(mTemperatureReadings.average());
    return DeviceOperationResult::ok;
}

void Ds18x20Driver::updateTempThread(std::stop_token token, Ds18x20Driver *instance) {
    using namespace std::chrono_literals;

    const auto *config  = reinterpret_cast<const ds18x20_driver_data *>(&instance->m_conf->device_config);
    while (!token.stop_requested()) {
        auto beforeReading = std::chrono::steady_clock::now();

        float temperature = 0.0f;
        Logger::log(LogLevel::Info, "Reading from gpio_num : %d @ address : %u%u", static_cast<int>(config->gpio),
            static_cast<uint32_t>(config->addr >> 32),
            static_cast<uint32_t>(config->addr & ((1ull << 32) - 1)));

        auto result = ds18x20_measure_and_read(config->gpio, config->addr, &temperature);
        instance->mTemperatureReadings.put_sample(temperature);

        if (result == ESP_OK) {
            Logger::log(LogLevel::Info, "Read temperature : %d", static_cast<int>(temperature * 1000));
        }

        const auto duration = std::chrono::steady_clock::now() - beforeReading;
        std::this_thread::sleep_for(duration < 5s ? 5s - duration : 500ms);
    }
    Logger::log(LogLevel::Info, "Exiting temperature thread");
}

// TODO: implement both
DeviceOperationResult Ds18x20Driver::get_info(char *output_buffer, size_t output_buffer_len) const {
    json_out out = JSON_OUT_BUF(output_buffer, output_buffer_len);
    json_printf(&out, "{}");
    return DeviceOperationResult::ok;
}

DeviceOperationResult Ds18x20Driver::call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::ok;
}

DeviceOperationResult Ds18x20Driver::update_runtime_data() {
    return DeviceOperationResult::ok;
}

bool Ds18x20Driver::add_address(ds18x20_addr_t address) {
    std::unique_lock instance_guard{_instance_mutex};

    bool adress_already_exists = std::any_of(Ds18x20Driver::_device_addresses.cbegin(), Ds18x20Driver::_device_addresses.cend(), 
        [&address](const auto &already_found_address) {
            return already_found_address.has_value() && *already_found_address == address;
        });

    if (adress_already_exists) {
        Logger::log(LogLevel::Warning, "No new address, don't add this address");
        return false;
    }

    auto first_empty_slot = std::find(Ds18x20Driver::_device_addresses.begin(), Ds18x20Driver::_device_addresses.end(), std::nullopt);

    // No free addresses
    if (first_empty_slot == Ds18x20Driver::_device_addresses.cend()) {
        return false;
    }

    *first_empty_slot = address;

    return true;
}

bool Ds18x20Driver::remove_address(ds18x20_addr_t address) {
    std::unique_lock instance_guard{_instance_mutex};

    auto found_address = std::find_if(Ds18x20Driver::_device_addresses.begin(), Ds18x20Driver::_device_addresses.end(), 
        [&address](auto &current_address) {
            return current_address.has_value() && *current_address == address;
        });

    if (found_address == Ds18x20Driver::_device_addresses.end()) {
        return false;
    }

    *found_address = std::nullopt;

    return true;
}