#include "pcf8575_driver.h"
#include "drivers/device_resource.h"
#include "drivers/device_types.h"
#include "utils/logger.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>
#include <string_view>
#include <optional>
#include <bit>

#include <pcf8575.h>

std::optional<Pcf8575Driver> Pcf8575Driver::create_driver(const DeviceConfig*config) {
    auto driver_data = config->accessConfig<Pcf8575DriverData>();
    auto sdaPin = DeviceResource::get_gpio_resource(driver_data->sdaPin, GpioPurpose::bus);
    auto sclPin = DeviceResource::get_gpio_resource(driver_data->sclPin, GpioPurpose::bus);

    if (!sdaPin || !sclPin) {
        Logger::log(LogLevel::Warning, "I2C Port cannot be used, since one or more pins are already reserved for a different purpose");
        return std::nullopt;
    }

    if (!add_address(driver_data->addr)) {
        Logger::log(LogLevel::Warning, "The device is already in use");
        return std::nullopt;
    }

    i2c_dev_t device{};
    // Has to be set specificaly
    device.cfg.clk_flags = 0;
    auto result = pcf8575_init_desc(&device, static_cast<uint8_t>(driver_data->addr), I2C_NUM_0, 
                    static_cast<gpio_num_t>(driver_data->sdaPin), 
                    static_cast<gpio_num_t>(driver_data->sclPin));

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't find any ads111x devices on port %d with address %d", 
            0, static_cast<int>(driver_data->addr));
        remove_address(driver_data->addr);
    }
    
    return Pcf8575Driver(config, std::move(device));
}

// TODO: maybe make pins configureable
std::optional<Pcf8575Driver> Pcf8575Driver::create_driver(const std::string_view input, DeviceConfig&device_conf_out) {
    Pcf8575Address address = Pcf8575Address::Invalid;
    unsigned int scl = static_cast<uint8_t>(sclDefaultPin);
    unsigned int sda = static_cast<uint8_t>(sdaDefaultPin);

    Logger::log(LogLevel::Info, "%.*s", input.size(), input.data());
    // TODO: write json_scanf_single
    json_scanf(input.data(), input.size(), R"({ address : %M, sclPin : %u, sdaPin : %u })", 
       json_scanf_single<decltype(address)>, &address, 
        &scl, &sda);

    if (address == Pcf8575Address::Invalid) {
        Logger::log(LogLevel::Warning, "Invalid address for pcf8575_driver : %d", (int) address);
        return std::nullopt;
    }

    Logger::log(LogLevel::Info, "Address for pcf8575_driver : %x", (int) address);

    auto sclPin = DeviceResource::get_gpio_resource(static_cast<gpio_num_t>(scl), GpioPurpose::bus);
    auto sdaPin = DeviceResource::get_gpio_resource(static_cast<gpio_num_t>(sda), GpioPurpose::bus);

    if (!sclPin || !sdaPin) {
        Logger::log(LogLevel::Warning, "GPIO pins couldn't be reserved");
        return std::nullopt;
    }

    if (!add_address(address)) {
        Logger::log(LogLevel::Warning, "Duplicate address, or address list is full");
        return std::nullopt;
    }

    i2c_dev_t device{};
    // Has to be set specificaly
    device.cfg.clk_flags = 0;
    auto result = pcf8575_init_desc(&device, static_cast<uint8_t>(address), I2C_NUM_0, static_cast<gpio_num_t>(sda), static_cast<gpio_num_t>(scl));

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't find any pcf8575 devices on port %d with address %d", 
            0, static_cast<int>(address));
        remove_address(address);
        return std::nullopt;
    }
    
    Logger::log(LogLevel::Info, "Initialized pcf8575 @ %x", (int) address);

    Pcf8575DriverData data { 
        .addr = address, 
        .sdaPin = static_cast<gpio_num_t>(sda),
        .sclPin = static_cast<gpio_num_t>(scl)
    };
    std::memcpy(device_conf_out.device_config.data(), &data, sizeof(Pcf8575DriverData));
    device_conf_out.device_driver_name =  Pcf8575Driver::name;

    return Pcf8575Driver(&device_conf_out, std::move(device));
}

Pcf8575Driver::Pcf8575Driver(const DeviceConfig*conf, i2c_dev_t device) 
    : m_conf(conf), m_device(std::move(device)) { 
}

Pcf8575Driver::Pcf8575Driver(Pcf8575Driver &&other) noexcept : m_conf(other.m_conf), m_device(other.m_device) {
    other.mReadingThread.request_stop();
    if (other.mReadingThread.joinable()) {
        other.mReadingThread.join();
    }

    other.m_conf = nullptr;
    std::memset(&other.m_device, 0, sizeof(i2c_dev_t));

    mReadingThread = std::jthread(&Pcf8575Driver::updatePinsThread, this);
 }

 Pcf8575Driver &Pcf8575Driver::operator=(Pcf8575Driver &&other) noexcept {
    using std::swap;

    other.mReadingThread.request_stop();
    if (other.mReadingThread.joinable()) {
        other.mReadingThread.join();
    }

    swap(m_conf, other.m_conf);
    swap(m_device, other.m_device);

    mReadingThread = std::jthread(&Pcf8575Driver::updatePinsThread, this);

    return *this;
}

Pcf8575Driver::~Pcf8575Driver() { 
    if (!m_conf) {
        return;
    }

    mReadingThread.request_stop();
    if (mReadingThread.joinable()) {
        mReadingThread.join();
    }
    remove_address(m_conf->accessConfig<Pcf8575DriverData>()->addr);
}

// TODO: fix that values can be written here
DeviceOperationResult Pcf8575Driver::write_value(std::string_view what, const DeviceValues &value) {
    auto config  = m_conf->accessConfig<Pcf8575DriverData>();

    uint8_t toSet = 0;
    uint8_t pin = 0xff;
    auto convResult = std::from_chars(what.data(), what.data() + what.size(), pin);

    if (convResult.ec != std::errc() || pin >= 16) {
        return DeviceOperationResult::failure;
    }

    if (value.enable().has_value()) {
        toSet = std::min<uint8_t>(*value.enable(), 1);
    } else if (value.percentage().has_value()) {
        toSet = std::min<uint8_t>(*value.percentage(), 1);
    } else {
        return DeviceOperationResult::failure;
    }

    // Only write values in here which are actually written. Otherwise reading a zero with port_read and then writing it, 
    // would convert an input to an output
    writtenValue &= std::numeric_limits<uint16_t>::max() & (toSet << pin);
    auto result = pcf8575_port_write(&m_device, config->pinValues);

    if (result != ESP_OK) {
        return DeviceOperationResult::failure;
    }

    return DeviceOperationResult::ok;
}

DeviceOperationResult Pcf8575Driver::read_value(std::string_view what, DeviceValues &value) const {
    auto config  = m_conf->accessConfig<const Pcf8575DriverData>();
    Logger::log(LogLevel::Info, "Reading address : %u value to read : %.*s", 
        static_cast<uint32_t>(config->addr), what.size(), what.data());

    uint8_t pin = 0xff;

    auto convResult = std::from_chars(what.data(), what.data() + what.size(), pin);

    if (convResult.ec != std::errc() || pin >= 16) {
        return DeviceOperationResult::failure;
    }

    value.enable(readValue & pin);

    return DeviceOperationResult::ok;
}

// TODO: implement both
DeviceOperationResult Pcf8575Driver::get_info(char *output_buffer, size_t output_buffer_len) const {
    json_out out = JSON_OUT_BUF(output_buffer, output_buffer_len);
    json_printf(&out, "{}");
    return DeviceOperationResult::ok;
}

DeviceOperationResult Pcf8575Driver::call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::ok;
}

DeviceOperationResult Pcf8575Driver::update_runtime_data() {
    return DeviceOperationResult::not_supported;
}

void Pcf8575Driver::updatePinsThread(std::stop_token token, Pcf8575Driver *instance) {
    using namespace std::chrono_literals;

    while(!token.stop_requested()) {
        auto beforeReading = std::chrono::steady_clock::now();
        
        uint16_t readValue = 0;
        auto result = pcf8575_port_read(&instance->m_device, &readValue);
        if (result == ESP_OK) {
            Logger::log(LogLevel::Info, "Read value : %u for pcf8575", static_cast<uint32_t>(readValue));
        } else {
            continue;
        }

        instance->readValue.exchange(readValue);

        const auto duration = std::chrono::steady_clock::now() - beforeReading;
        std::this_thread::sleep_for(duration < 5s ? 5s - duration : 500ms);
    }
    Logger::log(LogLevel::Info, "Exiting updatePinsThread");
}

bool Pcf8575Driver::add_address(Pcf8575Address address) {
    std::unique_lock instance_guard{_instance_mutex};

    bool adress_already_exists = std::any_of(Pcf8575Driver::_device_addresses.cbegin(), Pcf8575Driver::_device_addresses.cend(), 
        [&address](const auto &already_found_address) {
            return already_found_address.has_value() && *already_found_address == address;
        });

    if (adress_already_exists) {
        Logger::log(LogLevel::Warning, "No new address, don't add this address");
        return false;
    }

    auto first_empty_slot = std::find(Pcf8575Driver::_device_addresses.begin(), Pcf8575Driver::_device_addresses.end(), std::nullopt);

    // No free addresses
    if (first_empty_slot == Pcf8575Driver::_device_addresses.cend()) {
        return false;
    }

    Logger::log(LogLevel::Info, "Adding address : %x ", (int) address);
    *first_empty_slot = address;

    return true;
}

bool Pcf8575Driver::remove_address(Pcf8575Address address) {
    std::unique_lock instance_guard{_instance_mutex};

    auto found_address = std::find_if(Pcf8575Driver::_device_addresses.begin(), Pcf8575Driver::_device_addresses.end(), 
        [&address](auto &current_address) {
            return current_address.has_value() && *current_address == address;
        });

    if (found_address == Pcf8575Driver::_device_addresses.end()) {
        Logger::log(LogLevel::Info, "Couldn't remove address  : %x : not found ", (int) address);
        return false;
    }

    Logger::log(LogLevel::Info, "Removing address : %x ", (int) address);
    *found_address = std::nullopt;

    return true;
}