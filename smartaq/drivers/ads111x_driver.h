#pragma once

#include "ads111x.h"

#include <array>
#include <string_view>
#include <optional>
#include <memory>
#include <thread>
#include <cstdint>
#include <shared_mutex>

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "utils/container/sample_container.h"
#include "build_config.h"

enum struct Ads111xAddress : std::decay_t<decltype(ADS111X_ADDR_GND)> {
    GND = ADS111X_ADDR_GND,
    VCC = ADS111X_ADDR_VCC,
    SDA = ADS111X_ADDR_SDA,
    SCL = ADS111X_ADDR_SCL,
    INVALID = 0xFF
};

template<>
struct read_from_json<Ads111xAddress> {
    static void read(const char *str, int len, Ads111xAddress &unit) {
        std::string_view input(str, len);

        unit = Ads111xAddress::INVALID;

        if (input == "GND" || input == "gnd") {
            unit = Ads111xAddress::GND;
        } else if (input == "VCC" || input == "vcc") {
            unit = Ads111xAddress::VCC;
        } else if (input == "SDA" || input == "sda") {
            unit = Ads111xAddress::SDA;
        } else if (input == "SCL" || input == "scl") {
            unit = Ads111xAddress::SCL;
        }
    }
};

struct Ads111xDriverData final {
    Ads111xAddress addr;
    gpio_num_t sdaPin = static_cast<gpio_num_t>(sdaDefaultPin);
    gpio_num_t sclPin = static_cast<gpio_num_t>(sclDefaultPin);
};

class Ads111xDriver final {
    public:
        static inline constexpr char name[] = "ads111x_driver";
        static inline constexpr uint8_t MaxChannels = 4;

        Ads111xDriver(const Ads111xDriver &other) = delete;
        Ads111xDriver(Ads111xDriver &&other);
        ~Ads111xDriver();

        Ads111xDriver &operator=(const Ads111xDriver &other) = delete;
        Ads111xDriver &operator=(Ads111xDriver &&other);

        static std::optional<Ads111xDriver> create_driver(const std::string_view input, DeviceConfig&device_conf_out);
        static std::optional<Ads111xDriver> create_driver(const DeviceConfig*config);

        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data();
    private:
        Ads111xDriver(const DeviceConfig *conf, i2c_dev_t device, std::shared_ptr<GpioResource> sdaPin, std::shared_ptr<GpioResource> sclPin);
        static std::optional<Ads111xDriver> setupDevice(const DeviceConfig *conf, i2c_dev_t device,
                                                        std::shared_ptr<GpioResource> sdaPin,
                                                        std::shared_ptr<GpioResource> sclPin);

        static void updateAnalogThread(std::stop_token token, Ads111xDriver *instance);

        static bool addAddress(Ads111xAddress address);
        static bool removeAddress(Ads111xAddress address);

        const DeviceConfig*mConf;

        mutable i2c_dev_t mDevice;
        std::shared_ptr<GpioResource> mSdaPin{nullptr};
        std::shared_ptr<GpioResource> mSclPin{nullptr};
        std::jthread mAnalogReadingsThread;
        std::array<SampleContainer<uint16_t, uint16_t, 10>, MaxChannels> mAnalogReadings;

        static inline std::array<std::optional<Ads111xAddress>, 4> _device_addresses;
        static inline std::shared_mutex _instance_mutex;
};