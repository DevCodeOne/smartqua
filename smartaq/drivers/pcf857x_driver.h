#pragma once

#include "pcf8575.h"
#include "pcf8574.h"

#include <atomic>
#include <thread>
#include <limits>
#include <string_view>

#include "drivers/device_types.h"
#include "drivers/i2c_address_value.h"
#include "drivers/device_resource.h"
#include "utils/container/fixed_size_optional_array.h"
#include "build_config.h"
#include "utils/serialization/number_conversion.h"


struct Pcf8575DriverData final {
    I2cAddressValue addr;
    uint16_t pinValues = std::numeric_limits<uint16_t>::max();
    gpio_num_t sdaPin = static_cast<gpio_num_t>(sdaDefaultPin);
    gpio_num_t sclPin = static_cast<gpio_num_t>(sclDefaultPin);
};

struct Pcf8574DriverData final {
    I2cAddressValue addr;
    uint8_t pinValues = std::numeric_limits<uint8_t>::max();
    gpio_num_t sdaPin = static_cast<gpio_num_t>(sdaDefaultPin);
    gpio_num_t sclPin = static_cast<gpio_num_t>(sclDefaultPin);
};

template<typename DriverData>
struct Pcf857XInfo;

template<>
struct Pcf857XInfo<Pcf8575DriverData>
{
    static constexpr char name[] = "pcf8575_driver";

    static constexpr auto pcf_init_desc = pcf8575_init_desc;
    static constexpr auto pcf_port_write = pcf8575_port_write;
    static constexpr auto pcf_port_read = pcf8575_port_read;
};

template<>
struct Pcf857XInfo<Pcf8574DriverData>
{
    static constexpr char name[] = "pcf8574_driver";

    static constexpr auto pcf_init_desc = pcf8574_init_desc;
    static constexpr auto pcf_port_write = pcf8574_port_write;
    static constexpr auto pcf_port_read = pcf8574_port_read;
};


template<typename T>
concept IsValidIOExpanderData = std::is_same_v<T, Pcf8575DriverData> || std::is_same_v<T, Pcf8574DriverData>;

template<IsValidIOExpanderData DriverData>
class Pcf857XDriver final {
    public:
        using PcfInfo = Pcf857XInfo<DriverData>;
        static constexpr auto name = Pcf857XInfo<DriverData>::name;

        Pcf857XDriver(const Pcf857XDriver &other) = delete;
        Pcf857XDriver(Pcf857XDriver &&other) noexcept;
        ~Pcf857XDriver();

        Pcf857XDriver &operator=(const Pcf857XDriver &other) = delete;
        Pcf857XDriver &operator=(Pcf857XDriver &&other) noexcept;

        [[nodiscard]] static std::optional<Pcf857XDriver> create_driver(const std::string_view& input, DeviceConfig&deviceConfOut);
        [[nodiscard]] static std::optional<Pcf857XDriver> create_driver(const DeviceConfig*config);

        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data();
    private:
        Pcf857XDriver(const DeviceConfig*conf, i2c_dev_t device);
        static void updatePinsThread(std::stop_token token, Pcf857XDriver *instance);

        static bool add_address(I2cAddressValue address);
        static bool remove_address(I2cAddressValue address);

        const DeviceConfig *m_conf;

        mutable i2c_dev_t m_device;
        std::atomic_uint16_t readValue;
        uint16_t writtenValue = std::numeric_limits<uint16_t>::max();
        std::jthread mReadingThread;

        static inline FixedSizeOptionalArray<I2cAddressValue, 4> _device_addresses;
        static inline std::shared_mutex _instance_mutex;
};

template<IsValidIOExpanderData DriverData>
std::optional<Pcf857XDriver<DriverData>> Pcf857XDriver<DriverData>::create_driver(const DeviceConfig*config) {
    auto driver_data = config->accessConfig<DriverData>();
    auto i2cResource = DeviceResource::get_i2c_port(i2c_port_t::I2C_NUM_0, I2C_MODE_MASTER, driver_data->sdaPin, driver_data->sclPin);

    if (i2cResource == nullptr) {
        Logger::log(LogLevel::Warning, "I2C Port cannot be used, since one or more pins are already reserved for a different purpose");
        return {};
    }

    if (!add_address(driver_data->addr)) {
        Logger::log(LogLevel::Warning, "The device is already in use");
        return {};
    }

    i2c_dev_t device{};
    // Has to be set specifically
    device.cfg.clk_flags = 0;
    auto result = PcfInfo::pcf_init_desc(&device, static_cast<uint8_t>(driver_data->addr), I2C_NUM_0,
                                    driver_data->sdaPin,
                                    driver_data->sclPin);

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't find any %s devices on port %d with address %d",
            name, 0, static_cast<int>(driver_data->addr));
        remove_address(driver_data->addr);
    }

    return Pcf857XDriver{ config, device };
}

template<IsValidIOExpanderData DriverData>
std::optional<Pcf857XDriver<DriverData>> Pcf857XDriver<DriverData>::create_driver(const std::string_view& input, DeviceConfig &deviceConfOut) {
    I2cAddressValue address = I2cAddressValue::Invalid;
    unsigned int scl = static_cast<uint8_t>(sclDefaultPin);
    unsigned int sda = static_cast<uint8_t>(sdaDefaultPin);

    Logger::log(LogLevel::Info, "%.*s", input.size(), input.data());
    // TODO: write json_scanf_single
    json_scanf(input.data(), input.size(), R"({ address : %M, sclPin : %u, sdaPin : %u })",
       json_scanf_single<decltype(address)>, &address,
        &scl, &sda);

    if (address == I2cAddressValue::Invalid) {
        Logger::log(LogLevel::Warning, "Invalid address for %s : %d", name, (int) address);
        return std::nullopt;
    }

    Logger::log(LogLevel::Info, "Address for %s : %x", name, (int) address);

    DriverData data {
        .addr = address,
        .sdaPin = static_cast<gpio_num_t>(sda),
        .sclPin = static_cast<gpio_num_t>(scl)
    };

    deviceConfOut.insertConfig(&data);
    deviceConfOut.device_driver_name =  Pcf857XDriver::name;

    if (!add_address(address)) {
        Logger::log(LogLevel::Warning, "Duplicate address, or address list is full");
        return std::nullopt;
    }

    return create_driver(&deviceConfOut);
}

template<IsValidIOExpanderData DriverData>
Pcf857XDriver<DriverData>::Pcf857XDriver(const DeviceConfig *conf, i2c_dev_t device)
    : m_conf(conf), m_device(device) {
}

template<IsValidIOExpanderData DriverData>
Pcf857XDriver<DriverData>::Pcf857XDriver(Pcf857XDriver &&other) noexcept : m_conf(other.m_conf), m_device(other.m_device) {
    other.mReadingThread.request_stop();
    if (other.mReadingThread.joinable()) {
        other.mReadingThread.join();
    }

    other.m_conf = nullptr;
    std::memset(&other.m_device, 0, sizeof(i2c_dev_t));

    mReadingThread = std::jthread(&Pcf857XDriver::updatePinsThread, this);
 }

template<IsValidIOExpanderData DriverData>
Pcf857XDriver<DriverData>& Pcf857XDriver<DriverData>::operator=(Pcf857XDriver&& other) noexcept
{
    using std::swap;

    other.mReadingThread.request_stop();
    if (other.mReadingThread.joinable())
    {
        other.mReadingThread.join();
    }

    swap(m_conf, other.m_conf);
    swap(m_device, other.m_device);

    mReadingThread = std::jthread(&Pcf857XDriver::updatePinsThread, this);

    return *this;
}

template<IsValidIOExpanderData DriverData>
Pcf857XDriver<DriverData>::~Pcf857XDriver() {
    if (!m_conf) {
        return;
    }

    mReadingThread.request_stop();
    if (mReadingThread.joinable()) {
        mReadingThread.join();
    }
    remove_address(m_conf->accessConfig<DriverData>()->addr);
}

// TODO: fix that values can be written here
template<IsValidIOExpanderData DriverData>
DeviceOperationResult Pcf857XDriver<DriverData>::write_value(std::string_view what, const DeviceValues &value) {
    auto config  = m_conf->accessConfig<DriverData>();

    uint8_t toSet = 0;

    const auto pin = convertFromCharRange<uint8_t>(what);
    if (!pin.has_value() || pin >= 16)
    {
        return DeviceOperationResult::failure;
    }

    if (auto asEnable = value.getAsUnitAsType<DeviceValueUnit::enable>(); asEnable) {
        toSet = std::min<uint8_t>(*asEnable, 1);
    } else if (auto asPercentage = value.getAsUnitAsType<DeviceValueUnit::percentage>(); asPercentage) {
        toSet = std::min<uint8_t>(*asPercentage, 1);
    } else {
        return DeviceOperationResult::failure;
    }

    // Only write values in here which are actually written. Otherwise, reading a zero with port_read and then writing it,
    // would convert an input to an output
    writtenValue &= std::numeric_limits<uint16_t>::max() & (toSet << *pin);
    auto result = PcfInfo::pcf_port_write(&m_device, config->pinValues);

    if (result != ESP_OK) {
        return DeviceOperationResult::failure;
    }

    return DeviceOperationResult::ok;
}

template<IsValidIOExpanderData DriverData>
DeviceOperationResult Pcf857XDriver<DriverData>::read_value(std::string_view what, DeviceValues &value) const {
    auto config  = m_conf->accessConfig<const DriverData>();
    Logger::log(LogLevel::Info, "Reading address : %u value to read : %.*s",
        static_cast<uint32_t>(config->addr), what.size(), what.data());

    auto pin = convertFromCharRange<uint8_t>(what);

    if (!pin.has_value() || pin >= 16) {
        return DeviceOperationResult::failure;
    }

    value.setToUnit(DeviceValueUnit::enable, readValue & *pin);

    return DeviceOperationResult::ok;
}

// TODO: implement both
template<IsValidIOExpanderData DriverData>
DeviceOperationResult Pcf857XDriver<DriverData>::get_info(char *output_buffer, size_t output_buffer_len) const {
    json_out out = JSON_OUT_BUF(output_buffer, output_buffer_len);
    json_printf(&out, "{}");
    return DeviceOperationResult::ok;
}

template<IsValidIOExpanderData DriverData>
DeviceOperationResult Pcf857XDriver<DriverData>::call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::ok;
}

template<IsValidIOExpanderData DriverData>
DeviceOperationResult Pcf857XDriver<DriverData>::update_runtime_data() {
    return DeviceOperationResult::not_supported;
}

template<IsValidIOExpanderData DriverData>
void Pcf857XDriver<DriverData>::updatePinsThread(std::stop_token token, Pcf857XDriver *instance) {
    using namespace std::chrono_literals;

    using BitType = decltype(DriverData::pinValues);

    while(!token.stop_requested()) {
        auto beforeReading = std::chrono::steady_clock::now();

        BitType readValue = 0;
        auto result = PcfInfo::pcf_port_read(&instance->m_device, &readValue);
        if (result == ESP_OK) {
            Logger::log(LogLevel::Info, "Read value : %u for %s", static_cast<uint32_t>(readValue), name);
        } else {
            continue;
        }

        instance->readValue.exchange(readValue);

        const auto duration = std::chrono::steady_clock::now() - beforeReading;
        std::this_thread::sleep_for(duration < 5s ? 5s - duration : 500ms);
    }
    Logger::log(LogLevel::Info, "Exiting updatePinsThread");
}

template<IsValidIOExpanderData DriverData>
bool Pcf857XDriver<DriverData>::add_address(I2cAddressValue address) {
    std::unique_lock instance_guard{_instance_mutex};

    if (_device_addresses.contains(address)) {
        Logger::log(LogLevel::Warning, "No new address, don't add this address");
        return false;
    }

    if (!_device_addresses.append(address)) {
        Logger::log(LogLevel::Info, "Couldn't add address : %x, list is already full", (int) address);
        return false;
    }

    Logger::log(LogLevel::Info, "Adding address : %x ", (int) address);
    return true;
}

template<IsValidIOExpanderData DriverData>
bool Pcf857XDriver<DriverData>::remove_address(I2cAddressValue address) {
    std::unique_lock instance_guard{_instance_mutex};

    if (!_device_addresses.removeValue(address)) {
        Logger::log(LogLevel::Info, "Couldn't remove address  : %x : not found ", (int) address);
        return false;
    }

    Logger::log(LogLevel::Info, "Removing address : %x ", (int) address);

    return true;
}

using Pcf8575Driver = Pcf857XDriver<Pcf8575DriverData>;
using Pcf8574Driver = Pcf857XDriver<Pcf8574DriverData>;
