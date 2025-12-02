#include "pico_device_driver.h"

#include <cstdio>
#include <cstring>
#include <memory>

#include "hal/gpio_types.h"
#include "hal/i2c_types.h"
#include "i2cdev.h"
#include "driver/i2c.h"

#include "build_config.h"
#include "drivers/device_types.h"
#include "utils/serialization/number_conversion.h"

std::optional<PicoDeviceDriver> PicoDeviceDriver::create_driver(const DeviceConfig*config) {
    auto driver_data = config->accessConfig<PicoDeviceDriverData>();

    auto i2cResource = DeviceResource::get_i2c_port(i2c_port_t::I2C_NUM_0, I2C_MODE_MASTER,
                                                    driver_data->sdaPin, driver_data->sclPin);

    if (i2cResource == nullptr)
    {
        Logger::log(LogLevel::Error, "Couldn't acquire i2c bus");
        return {};
    }


    return setupDevice(config, i2cResource);
}

std::optional<PicoDeviceDriver> PicoDeviceDriver::create_driver(const std::string_view input, DeviceConfig&deviceConfOut) {
    PicoDeviceAddress address = PicoDeviceAddress::INVALID;
    unsigned int scl = static_cast<uint8_t>(sclDefaultPin);
    unsigned int sda = static_cast<uint8_t>(sdaDefaultPin);

    Logger::log(LogLevel::Info, "%.*s", input.size(), input.data());
    // TODO: write json_scanf_single
    json_scanf(input.data(), input.size(), R"({ address : %M, sclPin : %u, sdaPin : %u })", 
       json_scanf_single<decltype(address)>, &address, 
        &scl, &sda);

    if (address == PicoDeviceAddress::INVALID) {
        Logger::log(LogLevel::Warning, "Invalid address for pico_device_driver : %d", (int) address);
        return {};
    }

    Logger::log(LogLevel::Info, "Address for pico_device_driver : %x", (int) address);

    auto i2cResource = DeviceResource::get_i2c_port(i2c_port_t::I2C_NUM_0, I2C_MODE_MASTER,
                                                    static_cast<gpio_num_t>(sda), static_cast<gpio_num_t>(scl));

    if (i2cResource == nullptr)
    {
        Logger::log(LogLevel::Error, "Couldn't acquire i2c bus");
        return {};
    }

    PicoDeviceDriverData data { 
        .address = address, 
        .sdaPin = static_cast<gpio_num_t>(sda),
        .sclPin = static_cast<gpio_num_t>(scl)
    };
    deviceConfOut.insertConfig(&data);
    deviceConfOut.device_driver_name =  PicoDeviceDriver::name;

    return setupDevice(&deviceConfOut, i2cResource);
}

// TODO: Maybe add some kind of check, if devices are expected
std::optional<PicoDeviceDriver> PicoDeviceDriver::setupDevice(const DeviceConfig *config, std::shared_ptr<I2cResource> i2cResource) {
    auto driver_data = config->accessConfig<PicoDeviceDriverData>();

    if (!addAddress(driver_data->address)) {
        Logger::log(LogLevel::Warning, "The device is already in use");
        return {};
    }

    I2CDeviceType device{new i2c_dev_t};
    device->addr = static_cast<uint8_t>(driver_data->address);
    device->port = i2cResource->channel_num();
    device->cfg.mode = I2C_MODE_MASTER;
    device->cfg.sda_io_num = driver_data->sdaPin;
    device->cfg.scl_io_num = driver_data->sclPin;
    device->cfg.master.clk_speed = 100'000;
    device->cfg.clk_flags = 0;
    device->timeout_ticks = 17;
    device->cfg.sda_pullup_en = GPIO_PULLUP_DISABLE;
    device->cfg.scl_pullup_en = GPIO_PULLUP_DISABLE;

    auto i2cDevCreateSuccess = i2c_dev_create_mutex(device.get());

    if (i2cDevCreateSuccess != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't find any pico_devices on port %d with address %d",
            0, static_cast<int>(driver_data->address));
        removeAddress(driver_data->address);
        return std::nullopt;
    }

    Logger::log(LogLevel::Info, "Initialized pico_device_driver @ %x", static_cast<int>(driver_data->address));

    // TODO: Currently max memory size is 255 bytes, because of the address size of 1 byte, might change
    std::array<uint8_t, 255> deviceMemory{};
    Logger::log(LogLevel::Info, "Trying to read from device memory ...");

    const auto result = PicoDeviceDriver::readCompleteMemory(device.get(), 0u, deviceMemory.data(), deviceMemory.size());

    if (!result) {
        Logger::log(LogLevel::Info, "Device @ %x was not responding", static_cast<int>(device->addr));
        removeAddress(static_cast<PicoDeviceAddress>(device->addr));
        return std::nullopt;
    }

    Logger::log(LogLevel::Info, "Creating runtime access info ...");
    auto access = RuntimeAccessType::createRuntimeAccessFromInfo(deviceMemory);

    if (!access) {
        Logger::log(LogLevel::Warning, "Failed to create runtime info ...");
        removeAddress(static_cast<PicoDeviceAddress>(device->addr));
        return std::nullopt;
    }

    Logger::log(LogLevel::Info, "Successfully created device");

    return PicoDeviceDriver(config, *access, std::move(device), std::move(i2cResource));
}

PicoDeviceDriver::PicoDeviceDriver(const DeviceConfig* conf, RuntimeAccessType access, I2CDeviceType device,
                                   std::shared_ptr<I2cResource> resource)
    : mConf(conf), mResource(std::move(resource)), mDevice(std::move(device)), mAccess(std::move(access))
{
    device = nullptr;
}

PicoDeviceDriver::PicoDeviceDriver(PicoDeviceDriver &&other) noexcept : mConf(other.mConf), mDevice(std::move(other.mDevice)), mAccess(other.mAccess) {
    other.mDevice = nullptr;
    other.mConf = nullptr;
 }

 PicoDeviceDriver &PicoDeviceDriver::operator=(PicoDeviceDriver &&other) noexcept {
    using std::swap;

    swap(mConf, other.mConf);
    swap(mDevice, other.mDevice);
    swap(mAccess, other.mAccess);

    return *this;
}

PicoDeviceDriver::~PicoDeviceDriver() { 
    if (!mConf) {
        return;
    }

    removeAddress(mConf->accessConfig<PicoDeviceDriverData>()->address);
}

bool PicoDeviceDriver::readTagFromString(const std::string_view &what, unsigned int &index, BasicStackString<16> &tag)
{
    // Format is 12FPWM
    // index -> 12
    // tag -> FPWM
    auto copyString = SmallerBufferPoolType::get_free_buffer();

    if (what.size() >= copyString->size()) {
        // Shouldn't really happen
        return false;
    }

    std::strncpy(copyString->data(), what.data(), std::min(copyString->size(), what.size()));
    copyString->data()[what.size()] = '\0';
    const auto readValues = sscanf(copyString->data(), "%u%15s", &index, tag.data());

    if (readValues != 2)
    {
        Logger::log(LogLevel::Info, "Couldn't read values from string %.*s", what.size(), what.data());
        return false;
    }

    return true;
}

// TODO: only update specific devices instead of writing and reading the complete memory of the target device
// Here a mapping from name to type is needed
DeviceOperationResult PicoDeviceDriver::write_value(std::string_view what, const DeviceValues &value) {
    using namespace PicoDriver;

    if (what.empty()) {
        Logger::log(LogLevel::Info, "The param for write_value was empty");
        return DeviceOperationResult::failure;
    }

    const auto index = convertFromCharRange<unsigned int>(what);
    if (!index)
    {
        return DeviceOperationResult::failure;
    }

    if (*index > RuntimeAccess::MaxDevices) {
        Logger::log(LogLevel::Info, "Index %u greater than MaxDevices", index);
        return DeviceOperationResult::failure;
    }

    auto &currentDevice = mAccess[*index];
    Logger::log(LogLevel::Info, "Index : %u, Tag @ Index : %s", *index, currentDevice.tagName());

    return std::visit([&value, this]<typename MemoryRepresentation>(MemoryRepresentation &memoryRepresentation)
    {
        if constexpr (!std::is_same_v<MemoryRepresentation, std::monostate>)
        {
            return writeDeviceValueToPico(memoryRepresentation, value);
        } else
        {
            return DeviceOperationResult::not_supported;
        }
    }, currentDevice);
}

// Only read specific entry ?
DeviceOperationResult PicoDeviceDriver::read_value(std::string_view what, DeviceValues &value) const {
    Logger::log(LogLevel::Info, "ReadValue PicoDeviceDriver");
    using namespace PicoDriver;
    if (what.empty()) {
        Logger::log(LogLevel::Info, "What was empty");
        return DeviceOperationResult::failure;
    }

    BasicStackString<16> tag{};
    unsigned int index = 0;
    if (!readTagFromString(what.data(), index, tag))
    {
        return DeviceOperationResult::failure;
    }

    if (index > PicoDriver::RuntimeAccess::MaxDevices) {
        Logger::log(LogLevel::Info, "Index greater than MaxDevices");
        return DeviceOperationResult::failure;
    }

    // Partial updates don't work
    readCompleteMemory(mDevice.get(), 0, mAccess.rawData().data(), mAccess.rawData().size());
    const auto &currentDevice = mAccess[index];
    const auto tagView = tag.getStringView();
    if (tagView == ADCType::Name) {
        if (auto adc = std::get_if<MemoryRepresentation<ADCType> *>(&currentDevice); adc) {
            const auto rawValue = (*adc)->adcRawValue;
            // TODO: add a way to add both
            // value.setToUnit<DeviceValueUnit::voltage>(rawValue * MemoryRepresentation<ADCType>::ConversionFactor);
            value.setToUnit<DeviceValueUnit::generic_analog>(rawValue);
            Logger::log(LogLevel::Info, "Setting to value %u, %f", rawValue, *value.getAsUnitAsType<DeviceValueUnit::voltage>());
        }
    } else {
        Logger::log(LogLevel::Warning, "Couldn't find tag with name : %.*s", tagView.size(), tagView.data());
    }

    return DeviceOperationResult::ok;
}

DeviceOperationResult PicoDeviceDriver::get_info(char *output, size_t output_buffer_len) const {
    return DeviceOperationResult::ok;
}

DeviceOperationResult PicoDeviceDriver::call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json) {
    Logger::log(LogLevel::Info, "write value %.*s", action.size(), action.data());
    using namespace PicoDriver;

    if (action == "dump") {
        for (auto &currentByte : mAccess.rawData()) {
            Logger::log(LogLevel::Info, "%x", currentByte);
        }
    } else if (action == "discover") {
        unsigned int index = 0;
        for (auto &currentDeviceEntry : mAccess) {
            Logger::log(LogLevel::Info, "DeviceName: %u%s -> type index : %u", index, currentDeviceEntry.tagName(), currentDeviceEntry.index());
            ++index;
            // TODO: write to json output
        }
    }

    return DeviceOperationResult::ok;
}

bool PicoDeviceDriver::addAddress(PicoDeviceAddress address) {
    std::unique_lock instance_guard{_instanceMutex};

    bool adress_already_exists = std::any_of(PicoDeviceDriver::_deviceAddresses.cbegin(), PicoDeviceDriver::_deviceAddresses.cend(), 
        [&address](const auto &already_found_address) {
            return already_found_address.has_value() && *already_found_address == address;
        });

    if (adress_already_exists) {
        Logger::log(LogLevel::Warning, "No new address, don't add this address");
        return false;
    }

    auto first_empty_slot = std::find(PicoDeviceDriver::_deviceAddresses.begin(), PicoDeviceDriver::_deviceAddresses.end(), std::nullopt);

    // No free addresses
    if (first_empty_slot == PicoDeviceDriver::_deviceAddresses.cend()) {
        return false;
    }

    Logger::log(LogLevel::Info, "Adding address : %x ", (int) address);
    *first_empty_slot = address;

    return true;
}

bool PicoDeviceDriver::removeAddress(PicoDeviceAddress address) {
    std::unique_lock instance_guard{_instanceMutex};

    auto found_address = std::find_if(PicoDeviceDriver::_deviceAddresses.begin(), PicoDeviceDriver::_deviceAddresses.end(), 
        [&address](auto &current_address) {
            return current_address.has_value() && *current_address == address;
        });

    if (found_address == PicoDeviceDriver::_deviceAddresses.end()) {
        Logger::log(LogLevel::Info, "Couldn't remove address  : %x : not found ", (int) address);
        return false;
    }

    Logger::log(LogLevel::Info, "Removing address : %x ", (int) address);
    *found_address = std::nullopt;

    return true;
}

// TODO: check if address + size < 255
bool PicoDeviceDriver::readCompleteMemory(i2c_dev_t *device, uint8_t address, uint8_t *target, size_t targetSize) {
    auto doWork = [](i2c_dev_t *device, uint8_t address, uint8_t *target, size_t targetSize) -> esp_err_t {

        DoFinally giveMutex {
            [device]() {
                I2C_DEV_GIVE_MUTEX(device);

                return ESP_OK;
            }
        };

        I2C_DEV_TAKE_MUTEX(device);

        auto result = i2c_dev_read(device, &address, sizeof(address), target, targetSize);

        if (result != ESP_OK) {
            Logger::log(LogLevel::Info, "Couldn't read from pico_device ...");
            return ESP_FAIL;
        }

        return ESP_OK;
    };
    return doWork(device, address, target, targetSize) == ESP_OK;
}

