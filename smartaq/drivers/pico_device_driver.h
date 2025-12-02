#pragma once

#include <cstdint>
#include <array>
#include <optional>
#include <memory>
#include <shared_mutex>
#include <type_traits>

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "build_config.h"

#include "i2cdev.h"

#include "devices/pwm.h"
#include "devices/adc.h"
#include "devices/output.h"
#include "devices/drv8825.h"
#include "runtime_access.h"

enum struct PicoDeviceAddress : uint8_t { INVALID = 0xFF };

struct PicoDeviceDriverData {
    PicoDeviceAddress address;
    gpio_num_t sdaPin = static_cast<gpio_num_t>(sdaDefaultPin);
    gpio_num_t sclPin = static_cast<gpio_num_t>(sclDefaultPin);
};

template<>
struct read_from_json<PicoDeviceAddress> {
    static void read(const char *str, int len, PicoDeviceAddress &unit) {
        std::string_view input(str, len);

        auto intValue = static_cast<std::underlying_type_t<PicoDeviceAddress>>(PicoDeviceAddress::INVALID);
        // TODO: add alternative, where you could provide a string, when the from_chars conversion, doesn't work
        std::from_chars(str, str + len, intValue);

        unit = static_cast<PicoDeviceAddress>(intValue);
    }
};

struct FreeI2CDevice {
    void operator()(i2c_dev_t *device) {
        if (device) {
            // TODO: add this back
            // i2c_dev_delete_mutex(device);
        }
    }
};

class PicoDeviceDriver final {
    public:
        // TODO: Regenerate via tag names
        using RuntimeAccessType = PicoDriver::RuntimeAccess::RuntimeAccess<
        PicoDriver::DeviceList<
            PicoDriver::FixedPWMType,
            PicoDriver::ADCType,
            PicoDriver::StepperMotorTag<PicoDriver::NoDirectionPin, PicoDriver::PinUsed>, // StepperDosingPump, can't choose direction, but can be disabled
            PicoDriver::OutputType 
        >>;
        using I2CDeviceType = std::unique_ptr<i2c_dev_t, FreeI2CDevice>;

        static constexpr char name[] = "pico_dev_driver";

        PicoDeviceDriver(const PicoDeviceDriver &other) = delete;
        PicoDeviceDriver(PicoDeviceDriver &&other) noexcept;
        ~PicoDeviceDriver();

        PicoDeviceDriver &operator=(const PicoDeviceDriver &other) = delete;
        PicoDeviceDriver &operator=(PicoDeviceDriver &&other) noexcept;

        static std::optional<PicoDeviceDriver> create_driver(const std::string_view input, DeviceConfig&deviceConfOut);
        static std::optional<PicoDeviceDriver> create_driver(const DeviceConfig*config);

        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data() { return DeviceOperationResult::ok; }

    private:
        PicoDeviceDriver(const DeviceConfig *conf, RuntimeAccessType access, I2CDeviceType device, std::shared_ptr<I2cResource> resource);
        static std::optional<PicoDeviceDriver> setupDevice(const DeviceConfig *config, std::shared_ptr<I2cResource> i2cResource);

        template <typename TagType>
        DeviceOperationResult writeDeviceValueToPico(PicoDriver::MemoryRepresentation<TagType>* memoryRep,
                                                  const DeviceValues& value);

        static bool readCompleteMemory(i2c_dev_t *device, uint8_t address, uint8_t *target, size_t targetSize);
        template<typename TagType>
        bool writeCompleteMemory(i2c_dev_t *device, PicoDriver::MemoryRepresentation<TagType> *memorySlice);
        static bool readTagFromString(const std::string_view &what, unsigned int &index, BasicStackString<16> &tag);

        static bool addAddress(PicoDeviceAddress address);
        static bool removeAddress(PicoDeviceAddress address);

        const DeviceConfig *mConf = nullptr;
        std::shared_ptr<I2cResource> mResource = nullptr;

        mutable I2CDeviceType mDevice;
        mutable RuntimeAccessType mAccess;

        // TODO: make configurable
        static inline std::array<std::optional<PicoDeviceAddress>, 4> _deviceAddresses;
        static inline std::shared_mutex _instanceMutex;
};

template <typename TagType>
DeviceOperationResult PicoDeviceDriver::writeDeviceValueToPico(PicoDriver::MemoryRepresentation<TagType>* memoryRep,
                                                            const DeviceValues& value)
{
    using namespace PicoDriver;

    using DosingPumpDriver = StepperMotorTag<NoDirectionPin, PinUsed>;

    auto doWrite = [this](auto memoryRep)
    {
        if (writeCompleteMemory(mDevice.get(), memoryRep) != ESP_OK)
        {
            Logger::log(LogLevel::Warning, "Couldn't write to device");
            return DeviceOperationResult::failure;
        }
        return DeviceOperationResult::ok;
    };



    if constexpr (std::is_same_v<FixedPWMType, TagType>)
    {
        // TODO: add error handling
        memoryRep->pwmValue = value.getAsUnitAsType<DeviceValueUnit::generic_pwm>().value_or(0x1227);
        Logger::log(LogLevel::Info, "Found memory representation %s, writing %u", FixedPWMType::Name, static_cast<uint16_t>(memoryRep->pwmValue));
        return doWrite(memoryRep);
    }

    if constexpr (std::is_same_v<DosingPumpDriver, TagType>)
    {
        // TODO: add error handling
        memoryRep->steps = value.getAsUnitAsType<DeviceValueUnit::generic_unsigned_integral>().value_or(0);
        Logger::log(LogLevel::Info, "Found memory representation %s, writing %u", DosingPumpDriver::Name, static_cast<uint16_t>(memoryRep->steps));
        return doWrite(memoryRep);
    }

    if constexpr (std::is_same_v<OutputType, TagType>)
    {
        memoryRep->value = value.getAsUnitAsType<DeviceValueUnit::enable>().value_or(false);
        Logger::log(LogLevel::Info, "Found memory representation %s, writing %d", OutputType::Name, memoryRep->value);
        return doWrite(memoryRep);
    }

    Logger::log(LogLevel::Warning, "No write implementation for tag %s", TagType::Name);
    return DeviceOperationResult::not_supported;
}

// TODO: check if address + size < 255
template<typename TagType>
bool PicoDeviceDriver::writeCompleteMemory(i2c_dev_t *device, PicoDriver::MemoryRepresentation<TagType> *memorySlice) {
    auto slice = mAccess.toRawMemorySlice<TagType>(memorySlice);
    I2C_DEV_TAKE_MUTEX(device);

    DoFinally giveMutex{
        [device]()
        {
            I2C_DEV_GIVE_MUTEX(device);

            return ESP_OK;
        }
    };

    return i2c_dev_write(device, &slice.address, sizeof(slice.address), slice.data, slice.size);
}