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

std::optional<PicoDeviceDriver> PicoDeviceDriver::create_driver(const DeviceConfig*config) {
    auto driver_data = reinterpret_cast<const PicoDeviceDriverData *>(config->device_config.data());
    auto sdaPin = device_resource::get_gpio_resource(driver_data->sdaPin, gpio_purpose::bus);
    auto sclPin = device_resource::get_gpio_resource(driver_data->sclPin, gpio_purpose::bus);

    if (!sdaPin || !sclPin) {
        Logger::log(LogLevel::Warning, "I2C Port cannot be used, since one or more pins are already reserved for a different purpose");
        return std::nullopt;
    }

    if (!addAddress(driver_data->address)) {
        Logger::log(LogLevel::Warning, "The device is already in use");
        return std::nullopt;
    }

    I2CDeviceType device{new i2c_dev_t};
    device->addr = static_cast<uint8_t>(driver_data->address);
    device->port = I2C_NUM_0;
    device->cfg.mode = I2C_MODE_MASTER;
    device->cfg.sda_io_num = static_cast<gpio_num_t>(driver_data->sdaPin);
    device->cfg.scl_io_num = static_cast<gpio_num_t>(driver_data->sclPin);
    device->cfg.master.clk_speed = 400'000;
    device->cfg.clk_flags = 0;
    device->timeout_ticks = 17;
    device->cfg.sda_pullup_en = GPIO_PULLUP_DISABLE;
    device->cfg.scl_pullup_en = GPIO_PULLUP_DISABLE;

    auto result = i2c_dev_create_mutex(device.get());

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't find any devices on port %d with address %d", 
            0, static_cast<int>(driver_data->address));
        removeAddress(driver_data->address);
    }
    
    return setupDevice(config, std::move(device));
}

std::optional<PicoDeviceDriver> PicoDeviceDriver::create_driver(const std::string_view input, DeviceConfig&device_conf_out) {
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
        return std::nullopt;
    }

    Logger::log(LogLevel::Info, "Address for pico_device_driver : %x", (int) address);

    auto sclPin = device_resource::get_gpio_resource(static_cast<gpio_num_t>(scl), gpio_purpose::bus);
    auto sdaPin = device_resource::get_gpio_resource(static_cast<gpio_num_t>(sda), gpio_purpose::bus);

    if (!sclPin || !sdaPin) {
        Logger::log(LogLevel::Warning, "GPIO pins couldn't be reserved");
        return std::nullopt;
    }

    if (!addAddress(address)) {
        Logger::log(LogLevel::Warning, "Duplicate address, or address list is full");
        return std::nullopt;
    }

    I2CDeviceType device{new i2c_dev_t};
    device->addr = static_cast<uint8_t>(address);
    device->port = I2C_NUM_0;
    device->cfg.mode = I2C_MODE_MASTER;
    device->cfg.sda_io_num = static_cast<gpio_num_t>(sda);
    device->cfg.scl_io_num = static_cast<gpio_num_t>(scl);
    device->cfg.master.clk_speed = 400'000;
    device->cfg.clk_flags = 0;
    device->timeout_ticks = 17;
    device->cfg.sda_pullup_en = GPIO_PULLUP_DISABLE;
    device->cfg.scl_pullup_en = GPIO_PULLUP_DISABLE;

    auto result = i2c_dev_create_mutex(device.get());

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't find any pico_devices on port %d with address %d", 
            0, static_cast<int>(address));
        removeAddress(address);
        return std::nullopt;
    }
    
    Logger::log(LogLevel::Info, "Initialized pico_device_driver @ %x", (int) address);

    PicoDeviceDriverData data { 
        .address = address, 
        .sdaPin = static_cast<gpio_num_t>(sda),
        .sclPin = static_cast<gpio_num_t>(scl)
    };
    std::memcpy(reinterpret_cast<void *>(device_conf_out.device_config.data()), &data, sizeof(PicoDeviceDriverData));
    device_conf_out.device_driver_name =  PicoDeviceDriver::name;

    auto dev = setupDevice(&device_conf_out, std::move(device));

    Logger::log(LogLevel::Info, "After SetupDevice");

    return dev;
}

// TODO: Maybe add some kind of check, if devices are expected
std::optional<PicoDeviceDriver> PicoDeviceDriver::setupDevice(const DeviceConfig *device_conf_out, I2CDeviceType device) {
    // TODO: Currently max memory size is 255 bytes, because of the address size of 1 byte, might change
    std::array<uint8_t, 255> deviceMemory;
    Logger::log(LogLevel::Info, "Trying to read from device memory ...");

    auto result = PicoDeviceDriver::readCompleteMemory(device.get(), 0u, deviceMemory.data(), deviceMemory.size());

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

    return PicoDeviceDriver(device_conf_out, *access, std::move(device));
}

PicoDeviceDriver::PicoDeviceDriver(const DeviceConfig *conf, RuntimeAccessType access, I2CDeviceType device) 
    : mConf(conf), mDevice(std::move(device)), mAccess(std::move(access)) { 
    device = nullptr;
}

PicoDeviceDriver::PicoDeviceDriver(PicoDeviceDriver &&other) : mConf(other.mConf), mDevice(std::move(other.mDevice)), mAccess(other.mAccess) {
    other.mDevice = nullptr;
    other.mConf = nullptr;
 }

 PicoDeviceDriver &PicoDeviceDriver::operator=(PicoDeviceDriver &&other) {
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

    removeAddress(reinterpret_cast<PicoDeviceDriverData *>(mConf->device_config.data())->address);
}

// TODO: only update specific devices instead of writing and reading the complete memory of the target device
// Here a mapping from name to type is needed
DeviceOperationResult PicoDeviceDriver::write_value(std::string_view what, const DeviceValues &value) {
    using namespace PicoDriver;
    
    if (what.size() == 0) {
        Logger::log(LogLevel::Info, "What was empty");
        return DeviceOperationResult::failure;
    }

    stack_string<16> tag{};
    unsigned int index = 0;
    // Format is 12FPWM
    // index -> 12
    // tag -> FPWM
    auto copyString = SmallerBufferPoolType::get_free_buffer();

    if (what.size() >= copyString->size()) {
        // Shouldn't really happen
        return DeviceOperationResult::failure;
    }

    std::strncpy(copyString->data(), what.data(), std::min(copyString->size(), what.size()));
    copyString->data()[what.size()] = '\0';
    sscanf(copyString->data(), "%u%15s", &index, tag.data());


    if (index > PicoDriver::RuntimeAccess::MaxDevices) {
        Logger::log(LogLevel::Info, "Index %u greater than MaxDevices, Tag was %s", index, tag);
        return DeviceOperationResult::failure;
    }

    const auto &currentDevice = mAccess[index];
    Logger::log(LogLevel::Info, "Tag %.*s and Index : %u, Tag @ Index :%s", tag.len(), tag.data(), index, currentDevice.tagName());
    if (tag.getStringView() == FixedPWMType::Name) {
        if (auto pwm = std::get_if<MemoryRepresentation<FixedPWMType> *>(&currentDevice); pwm) {
            Logger::log(LogLevel::Info, "Found memory representation");
            // TODO: add error handling
            (*pwm)->pwmValue = value.generic_pwm().value_or(0x1227);
            const auto update = mAccess.toRawMemorySlice<FixedPWMType>(*pwm);
            writeCompleteMemory(mDevice.get(), update.address, update.data, update.size);
            Logger::log(LogLevel::Info, "Writing update");
        } else {
            Logger::log(LogLevel::Info, "Memory Representation was wrong ...");
        }
    } else if (tag.getStringView() == PicoDriver::StepperMotorTag<PicoDriver::NoDirectionPin, PicoDriver::PinUsed>::Name) {
        if (auto stepper = 
            std::get_if<MemoryRepresentation<PicoDriver::StepperMotorTag<PicoDriver::NoDirectionPin, PicoDriver::PinUsed> > *>(&currentDevice); 
            stepper) {
            Logger::log(LogLevel::Info, "Found memory representation");
            // TODO: add error handling
            (*stepper)->steps = value.generic_unsigned_integral().value_or(0);
            const auto update = mAccess.toRawMemorySlice<PicoDriver::StepperMotorTag<PicoDriver::NoDirectionPin, PicoDriver::PinUsed>>(*stepper);
            writeCompleteMemory(mDevice.get(), update.address, update.data, update.size);
            unsigned int steps = (*stepper)->steps;
            Logger::log(LogLevel::Info, "Writing update, added %u steps %u", steps, value.generic_unsigned_integral().value_or(0));
        } else {
            Logger::log(LogLevel::Info, "Memory Representation was wrong ...");
        }
    }
    else {
    }

    return DeviceOperationResult::ok;
}

// Only read specific entry ?
DeviceOperationResult PicoDeviceDriver::read_value(std::string_view what, DeviceValues &value) const {
    Logger::log(LogLevel::Info, "ReadValue PicoDeviceDriver");
    const auto *config  = reinterpret_cast<const PicoDeviceDriverData *>(&mConf->device_config);
    using namespace PicoDriver;
    if (what.size() == 0) {
        Logger::log(LogLevel::Info, "What was empty");
        return DeviceOperationResult::failure;
    }

    stack_string<16> tag{};
    unsigned int index = 0;
    // Format is 12FPWM
    // index -> 12
    // tag -> FPWM
    auto copyString = SmallerBufferPoolType::get_free_buffer();

    if (what.size() >= copyString->size()) {
        // Shouldn't really happen
        return DeviceOperationResult::failure;
    }

    std::strncpy(copyString->data(), what.data(), std::min(copyString->size(), what.size()));
    copyString->data()[what.size()] = '\0';
    sscanf(copyString->data(), "%u%15s", &index, tag.data());


    if (index > PicoDriver::RuntimeAccess::MaxDevices) {
        Logger::log(LogLevel::Info, "Index %u greater than MaxDevices, Tag was %s", index, tag);
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
            Logger::log(LogLevel::Info, "Setting to value %u, %f", rawValue, *value.getAsUnit<DeviceValueUnit::voltage>());
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

        auto result = i2c_dev_read(device, reinterpret_cast<void *>(&address), sizeof(address),
                                        reinterpret_cast<void *>(target), targetSize);

        if (result != ESP_OK) {
            Logger::log(LogLevel::Info, "Couldn't read from pico_device ...");
            return ESP_FAIL;
        }

        return ESP_OK;
    };

    return doWork(device, address, target, targetSize) == ESP_OK;
}

// TODO: check if address + size < 255
bool PicoDeviceDriver::writeCompleteMemory(i2c_dev_t *device, uint8_t address, const uint8_t *target, size_t targetSize) {
    auto doWork = [](i2c_dev_t *device, uint8_t address, const uint8_t *target, size_t targetSize) -> esp_err_t {
        I2C_DEV_TAKE_MUTEX(device);

        DoFinally giveMutex { 
            [device]() {
                I2C_DEV_GIVE_MUTEX(device);

                return ESP_OK;
            }
        };

        const auto result = i2c_dev_write(device, reinterpret_cast<void *>(&address), sizeof(address), 
                                            reinterpret_cast<const void *>(target), targetSize);

        if (result != ESP_OK) {
            return ESP_FAIL;
        }

        return ESP_FAIL;
    };

    return doWork(device, address, target, targetSize) == ESP_OK;
}