#include "ads111x_driver.h"
#include "utils/logger.h"

#include <algorithm>
#include <thread>
#include <utility>
#include <string_view>
#include <optional>

#include <ads111x.h>

std::optional<Ads111xDriver> Ads111xDriver::create_driver(const device_config *config) {
    auto driver_data = reinterpret_cast<const Ads111xDriverData *>(config->device_config.data());
    auto sdaPin = device_resource::get_gpio_resource(driver_data->sdaPin, gpio_purpose::bus);
    auto sclPin = device_resource::get_gpio_resource(driver_data->sclPin, gpio_purpose::bus);

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
    auto result = ads111x_init_desc(&device, static_cast<uint8_t>(driver_data->addr), 0, 
                    static_cast<gpio_num_t>(driver_data->sdaPin), 
                    static_cast<gpio_num_t>(driver_data->sclPin));

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't find any ads111x devices on port %d with address %d", 
            0, static_cast<int>(driver_data->addr));
        remove_address(driver_data->addr);
    }
    
    return setupDevice(config, std::move(device));
}

// TODO: maybe make pins configureable
std::optional<Ads111xDriver> Ads111xDriver::create_driver(const std::string_view input, device_config &device_conf_out) {
    Ads111xAddress address = Ads111xAddress::INVALID;
    unsigned int scl = static_cast<uint8_t>(Ads111xDriverData::sclDefaultPin);
    unsigned int sda = static_cast<uint8_t>(Ads111xDriverData::sdaDefaultPin);

    Logger::log(LogLevel::Info, "%.*s", input.size(), input.data());
    // TODO: write json_scanf_single
    json_scanf(input.data(), input.size(), R"({ address : %M, sclPin : %u, sdaPin : %u })", 
       json_scanf_single<decltype(address)>, &address, 
        &scl, &sda);

    if (address == Ads111xAddress::INVALID) {
        Logger::log(LogLevel::Warning, "Invalid address for ads111x_driver : %d", (int) address);
        return std::nullopt;
    }

    Logger::log(LogLevel::Info, "Address for ads111x_driver : %x", (int) address);

    auto sclPin = device_resource::get_gpio_resource(static_cast<gpio_num_t>(scl), gpio_purpose::bus);
    auto sdaPin = device_resource::get_gpio_resource(static_cast<gpio_num_t>(sda), gpio_purpose::bus);

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
    auto result = ads111x_init_desc(&device, static_cast<uint8_t>(address), 0, static_cast<gpio_num_t>(sda), static_cast<gpio_num_t>(scl));

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't find any ads111x devices on port %d with address %d", 
            0, static_cast<int>(address));
        remove_address(address);
        return std::nullopt;
    }
    
    Logger::log(LogLevel::Info, "Initialized ads111x @ %x", (int) address);

    Ads111xDriverData data { 
        .addr = address, 
        .sdaPin = static_cast<gpio_num_t>(sda),
        .sclPin = static_cast<gpio_num_t>(scl)
    };
    std::memcpy(reinterpret_cast<void *>(device_conf_out.device_config.data()), &data, sizeof(Ads111xDriverData));
    device_conf_out.device_driver_name =  Ads111xDriver::name;

    return setupDevice(&device_conf_out, std::move(device));
}

std::optional<Ads111xDriver> Ads111xDriver::setupDevice(const device_config *device_conf_out, i2c_dev_t device) {
    auto result = ads111x_set_mode(&device, ADS111X_MODE_CONTINUOUS);

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't set ads111x to continous mode");
        remove_address(reinterpret_cast<Ads111xDriverData *>(device_conf_out->device_config.data())->addr);
        return std::nullopt;
    }

    ads111x_set_data_rate(&device, ADS111X_DATA_RATE_32);
    ads111x_set_gain(&device, ADS111X_GAIN_4V096);

    return Ads111xDriver(device_conf_out, std::move(device));
}

Ads111xDriver::Ads111xDriver(const device_config *conf, i2c_dev_t device) 
    : m_conf(conf), m_device(std::move(device)) { }

Ads111xDriver::Ads111xDriver(Ads111xDriver &&other) : m_conf(other.m_conf), m_device(other.m_device) {
    other.m_conf = nullptr;
    std::memset(reinterpret_cast<void *>(&other.m_device), 0, sizeof(i2c_dev_t));
 }

 Ads111xDriver &Ads111xDriver::operator=(Ads111xDriver &&other) {
    using std::swap;

    swap(m_conf, other.m_conf);
    swap(m_device, other.m_device);

    return *this;
}

Ads111xDriver::~Ads111xDriver() { 
    if (m_conf) {
        remove_address(reinterpret_cast<Ads111xDriverData *>(m_conf->device_config.data())->addr);
    }
}

DeviceOperationResult Ads111xDriver::write_value(const device_values &value) { 
    return DeviceOperationResult::not_supported;
}

// TODO: read sample value and sample values in update_runtime_data, or even in a seperate thread
// TODO: This device can read four channels, this has to return 4 device_values
DeviceOperationResult Ads111xDriver::read_value(std::string_view what, device_values &value) const {
    const auto *config  = reinterpret_cast<const Ads111xDriverData *>(&m_conf->device_config);
    int16_t analog = 0;
    Logger::log(LogLevel::Info, "Reading address : %u value to read : %.*s", static_cast<uint32_t>(config->addr), what.size(), what.data());

    if (what == "a0") {
        ads111x_set_input_mux(&m_device, ADS111X_MUX_0_GND);
    } else if (what == "a1") {
        ads111x_set_input_mux(&m_device, ADS111X_MUX_1_GND);
    } else if (what == "a2") {
        ads111x_set_input_mux(&m_device, ADS111X_MUX_2_GND);
    } else if (what == "a3") {
        ads111x_set_input_mux(&m_device, ADS111X_MUX_3_GND);
    }

    bool busy = false;

    for (int i = 0; i < 3 && !busy; ++i) { 
        using namespace std::chrono_literals;
        ads111x_is_busy(&m_device, &busy); 
        std::this_thread::sleep_for(1s);
    }

    auto result = ads111x_get_value(&m_device, &analog);

    if (result != ESP_OK) {
        return DeviceOperationResult::failure;
    }

    Logger::log(LogLevel::Info, "Read analog value : %u", static_cast<uint32_t>(analog));
    
    value.generic_analog = analog;

    return DeviceOperationResult::ok;
}

// TODO: implement both
DeviceOperationResult Ads111xDriver::get_info(char *output_buffer, size_t output_buffer_len) const {
    json_out out = JSON_OUT_BUF(output_buffer, output_buffer_len);
    json_printf(&out, "{}");
    return DeviceOperationResult::ok;
}

DeviceOperationResult Ads111xDriver::call_device_action(device_config *conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::ok;
}

DeviceOperationResult Ads111xDriver::update_runtime_data() {
    return DeviceOperationResult::ok;
}

bool Ads111xDriver::add_address(Ads111xAddress address) {
    std::unique_lock instance_guard{_instance_mutex};

    bool adress_already_exists = std::any_of(Ads111xDriver::_device_addresses.cbegin(), Ads111xDriver::_device_addresses.cend(), 
        [&address](const auto &already_found_address) {
            return already_found_address.has_value() && *already_found_address == address;
        });

    if (adress_already_exists) {
        Logger::log(LogLevel::Warning, "No new address, don't add this address");
        return false;
    }

    auto first_empty_slot = std::find(Ads111xDriver::_device_addresses.begin(), Ads111xDriver::_device_addresses.end(), std::nullopt);

    // No free addresses
    if (first_empty_slot == Ads111xDriver::_device_addresses.cend()) {
        return false;
    }

    Logger::log(LogLevel::Info, "Adding address : %x ", (int) address);
    *first_empty_slot = address;

    return true;
}

bool Ads111xDriver::remove_address(Ads111xAddress address) {
    std::unique_lock instance_guard{_instance_mutex};

    auto found_address = std::find_if(Ads111xDriver::_device_addresses.begin(), Ads111xDriver::_device_addresses.end(), 
        [&address](auto &current_address) {
            return current_address.has_value() && *current_address == address;
        });

    if (found_address == Ads111xDriver::_device_addresses.end()) {
        Logger::log(LogLevel::Info, "Couldn't remove address  : %x : not found ", (int) address);
        return false;
    }

    Logger::log(LogLevel::Info, "Removing address : %x ", (int) address);
    *found_address = std::nullopt;

    return true;
}