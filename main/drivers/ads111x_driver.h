#pragma once

#include "ads111x.h"

#include <array>
#include <string_view>
#include <optional>
#include <memory>
#include <shared_mutex>

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "smartqua_config.h"

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
    constexpr static inline auto sdaDefaultPin = GPIO_NUM_21;
    constexpr static inline auto sclDefaultPin = GPIO_NUM_22;

    Ads111xAddress addr;
    gpio_num_t sdaPin = sdaDefaultPin;
    gpio_num_t sclPin = sclDefaultPin;
};

class Ads111xDriver final {
    public:
        static inline constexpr char name[] = "ads111x_driver";

        Ads111xDriver(const Ads111xDriver &other) = delete;
        Ads111xDriver(Ads111xDriver &&other);
        ~Ads111xDriver();

        Ads111xDriver &operator=(const Ads111xDriver &other) = delete;
        Ads111xDriver &operator=(Ads111xDriver &&other);

        static std::optional<Ads111xDriver> create_driver(const std::string_view input, device_config &device_conf_out);
        static std::optional<Ads111xDriver> create_driver(const device_config *config);

        DeviceOperationResult write_value(const device_values &value);
        DeviceOperationResult read_value(std::string_view what, device_values &value) const;
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult call_device_action(device_config *conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data();
    private:
        Ads111xDriver(const device_config *conf, i2c_dev_t device);
        static std::optional<Ads111xDriver> setupDevice(const device_config *conf, i2c_dev_t device);

        static bool add_address(Ads111xAddress address);
        static bool remove_address(Ads111xAddress address);

        const device_config *m_conf;
        mutable i2c_dev_t m_device;

        static inline std::array<std::optional<Ads111xAddress>, 4> _device_addresses;
        static inline std::shared_mutex _instance_mutex;
};