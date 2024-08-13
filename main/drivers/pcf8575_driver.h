#pragma once

#include "pcf8575.h"

#include <atomic>
#include <cstdint>
#include <charconv>
#include <thread>
#include <limits>
#include <string_view>
#include <type_traits>

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "build_config.h"

enum struct Pcf8575Address : uint8_t { Invalid = 0xFF };

template<>
struct read_from_json<Pcf8575Address> {
    static void read(const char *str, int len, Pcf8575Address &addr) {
        std::string_view input(str, len);
        int base = 10;

        addr = Pcf8575Address::Invalid;
        const char *start = str;

        if (input.starts_with("0b")) {
            str += 2;
            base = 2;
        } else if (input.starts_with("0x")) {
            str += 2;
            base = 16;
        } else if (input.starts_with("0")) {
            str += 1;
            base = 8;
        } 

        const char *end = str + len;
        auto asIntValue{static_cast<std::underlying_type_t<Pcf8575Address>>(Pcf8575Address::Invalid)};
        auto result = std::from_chars(start, end, asIntValue, base);

        if (result.ec == std::errc()) {
            addr = static_cast<Pcf8575Address>(asIntValue + 0x20);
        }
    }
};

struct Pcf8575DriverData final {
    Pcf8575Address addr;
    uint16_t pinValues = std::numeric_limits<uint16_t>::max();
    gpio_num_t sdaPin = static_cast<gpio_num_t>(sdaDefaultPin);
    gpio_num_t sclPin = static_cast<gpio_num_t>(sclDefaultPin);
};

class Pcf8575Driver final {
    public:
        static inline constexpr char name[] = "pcf8575_driver";

        Pcf8575Driver(const Pcf8575Driver &other) = delete;
        Pcf8575Driver(Pcf8575Driver &&other);
        ~Pcf8575Driver();

        Pcf8575Driver &operator=(const Pcf8575Driver &other) = delete;
        Pcf8575Driver &operator=(Pcf8575Driver &&other);

        static std::optional<Pcf8575Driver> create_driver(const std::string_view input, DeviceConfig&device_conf_out);
        static std::optional<Pcf8575Driver> create_driver(const DeviceConfig*config);

        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data();
    private:
        Pcf8575Driver(const DeviceConfig*conf, i2c_dev_t device);
        static void updatePinsThread(std::stop_token token, Pcf8575Driver *instance);

        static bool add_address(Pcf8575Address address);
        static bool remove_address(Pcf8575Address address);

        const DeviceConfig*m_conf;

        mutable i2c_dev_t m_device;
        std::atomic_uint16_t readValue;
        uint16_t writtenValue = std::numeric_limits<uint16_t>::max();
        std::jthread mReadingThread;

        static inline std::array<std::optional<Pcf8575Address>, 4> _device_addresses;
        static inline std::shared_mutex _instance_mutex;
};