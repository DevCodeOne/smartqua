#include "ds18x20_driver.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>
#include <utility>

#include "frozen.h"
#include "ds18x20.h"

#include "build_config.h"

std::optional<Ds18x20Driver> Ds18x20Driver::create_driver(const DeviceConfig *config) {
    auto driverData = config->accessConfig<Ds18x20DriverData >();
    auto pin = DeviceResource::get_gpio_resource(driverData->gpio, GpioPurpose::bus);

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

    if (!addAddress(driverData->addr)) {
        Logger::log(LogLevel::Warning, "The device is already in use");
        return std::nullopt;
    }
    
    return Ds18x20Driver(config, pin);
}

std::optional<Ds18x20Driver> Ds18x20Driver::create_driver(const std::string_view& input, DeviceConfig& deviceConfOut)
{
    std::array<ds18x20_addr_t, max_num_devices> sensorAddresses{};
    int gpioNum = -1;
    json_scanf(input.data(), input.size(), R"({ gpio_num : %d})", &gpioNum);
    Logger::log(LogLevel::Info, "gpio_num : %d", gpioNum);

    if (gpioNum == -1) {
        Logger::log(LogLevel::Warning, "Invalid gpio num : %d", gpioNum);
        return std::nullopt;
    }

    auto pin = DeviceResource::get_gpio_resource(static_cast<gpio_num_t>(gpioNum), GpioPurpose::bus);

    if (!pin) {
        Logger::log(LogLevel::Warning, "GPIO pin couldn't be reserved");
        return std::nullopt;
    }

    size_t numDevicesFound = -1;
    auto result = ds18x20_scan_devices(static_cast<gpio_num_t>(gpioNum), sensorAddresses.data(), max_num_devices,
                                       &numDevicesFound);

    if (result != ESP_OK || numDevicesFound < 1)
    {
        Logger::log(LogLevel::Warning, "Didn't find any devices on port : %d, result : %d, num_devices : %d", gpioNum,
                    (int)result, (int)numDevicesFound);
        return std::nullopt;
    }

    // Skip already found addresses and use only the new ones
    std::optional<unsigned int> indexToAdd{};

    for (unsigned int i = 0; i < numDevicesFound; ++i) {
        if (addAddress(sensorAddresses[i])) {
            indexToAdd = i;
            break;
        }
    }

    if (!indexToAdd.has_value()) {
        Logger::log(LogLevel::Info, "Didn't find any devices attached to gpio_num : %d", gpioNum);
        return std::nullopt;
    }

    Logger::log(LogLevel::Info, "Found devices on gpio_num : %d @ address : %llu", gpioNum, sensorAddresses[*indexToAdd]);

    Ds18x20DriverData data { static_cast<gpio_num_t>(gpioNum), sensorAddresses[*indexToAdd] };
    deviceConfOut.insertConfig(&data);

    return Ds18x20Driver(&deviceConfOut, pin);
}

Ds18x20Driver::Ds18x20Driver(const DeviceConfig *conf, std::shared_ptr<GpioResource> pin) : mConf(conf), mPin(std::move(pin)) {
}

Ds18x20Driver::Ds18x20Driver(Ds18x20Driver &&other) noexcept : mConf(other.mConf), mPin(std::move(other.mPin)) {
    other.mConf = nullptr;
    other.mPin = nullptr;
 }

 Ds18x20Driver &Ds18x20Driver::operator=(Ds18x20Driver &&other) noexcept {
    using std::swap;

    swap(mConf, other.mConf);
    swap(mPin, other.mPin);

    return *this;
}

Ds18x20Driver::~Ds18x20Driver() { 
    if (mConf == nullptr) {
        return;
    }

    Logger::log(LogLevel::Info, "Deleting instance of Ds18x20Driver");
    removeAddress(mConf->accessConfig<Ds18x20DriverData>()->addr);
}

DeviceOperationResult Ds18x20Driver::read_value(std::string_view what, DeviceValues &value) const {
    value.setToUnit(DeviceValueUnit::temperature, mTemperatureReadings.average());
    return DeviceOperationResult::ok;
}

// TODO: implement both
DeviceOperationResult Ds18x20Driver::get_info(char *output_buffer, size_t output_buffer_len) const {
    json_out out = JSON_OUT_BUF(output_buffer, output_buffer_len);
    json_printf(&out, "{}");
    return DeviceOperationResult::ok;
}

DeviceState Ds18x20Driver::oneIteration()
{
    using namespace std::chrono_literals;

    const auto *config  = mConf->accessConfig<Ds18x20DriverData>();

    float temperature = 0.0f;
    Logger::log(LogLevel::Info, "Reading from gpio_num : %d @ address : %u%u", static_cast<int>(config->gpio),
        static_cast<uint32_t>(config->addr >> 32),
        static_cast<uint32_t>(config->addr & ((1ull << 32) - 1)));

    auto result = ds18x20_measure_and_read(config->gpio, config->addr, &temperature);

    if (result != ESP_OK)
    {
        Logger::log(LogLevel::Warning, "Couldn't read temperature");
        return DeviceState::ReadError;
    }

    Logger::log(LogLevel::Info, "Read temperature : %d", static_cast<int>(temperature * 1000));
    if (!mTemperatureReadings.putSample(temperature))
    {
        Logger::log(LogLevel::Warning, "There was an issue with the sample");
        return DeviceState::SampleIssue;
    }

    return DeviceState::Ok;
}

bool Ds18x20Driver::reinit()
{
    // TODO: Maybe just read new value and see if there was an issue?
    return true;
}

bool Ds18x20Driver::addAddress(ds18x20_addr_t address) {
    std::unique_lock instance_guard{_instanceMutex};

    if (_deviceAddresses.contains(address)) {
        Logger::log(LogLevel::Warning, "No new address, don't add this address");
        return false;
    }

    return _deviceAddresses.append(address);
}

bool Ds18x20Driver::removeAddress(ds18x20_addr_t address) {
    std::unique_lock instance_guard{_instanceMutex};

    if (!_deviceAddresses.removeValue(address)) {
        Logger::log(LogLevel::Warning, "Couldn't remove address, wasn't in the list");
    }

    return true;
}