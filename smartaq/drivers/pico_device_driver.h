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

        static inline constexpr char name[] = "pico_dev_driver";

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
        PicoDeviceDriver(const DeviceConfig *conf, RuntimeAccessType access, I2CDeviceType device);
        static std::optional<PicoDeviceDriver> setupDevice(const DeviceConfig *config, I2CDeviceType device);

        static bool readCompleteMemory(i2c_dev_t *device, uint8_t address, uint8_t *target, size_t targetSize);
        static bool writeCompleteMemory(i2c_dev_t *device, uint8_t address, const uint8_t *target, size_t targetSize);

        static bool addAddress(PicoDeviceAddress address);
        static bool removeAddress(PicoDeviceAddress address);

        const DeviceConfig *mConf = nullptr;

        mutable I2CDeviceType mDevice;
        mutable RuntimeAccessType mAccess;

        // TODO: make configurable
        static inline std::array<std::optional<PicoDeviceAddress>, 4> _deviceAddresses;
        static inline std::shared_mutex _instanceMutex;
};