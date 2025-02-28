#pragma once

#include <cstdint>
#include <memory>
#include <thread>
#include <string_view>
#include <thread>
#include <expected>

#include "build_config.h"
#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "utils/stack_string.h"
#include "build_config.h"

#include "driver/rmt_types.h"


enum struct SwitchType {
    LevelHolder,
    Invalid
};

enum struct SwitchDefaultValue {
    Low, High,
    Invalid
};

template<>
struct read_from_json<SwitchType> {
    static void read(const char *str, int len, SwitchType &type) {
        std::string_view input(str, len);

        type = SwitchType::Invalid;

        if (input == "LevelHolder") {
            type = SwitchType::LevelHolder;
        }
    }
};

template<>
struct read_from_json<SwitchDefaultValue> {
    static void read(const char *str, int len, SwitchDefaultValue &value) {
        std::string_view input(str, len);

        value = SwitchDefaultValue::Invalid;

        if (input == "Low" || input == "low") {
            value = SwitchDefaultValue::Low;
        } else if (input == "High" || input == "high") {
            value = SwitchDefaultValue::High;
        }
    }
};

// TODO: optional add backup probe and control device
// TODO: also add some kind of alarm, when both devices report different values, which are too different
struct SwitchConfig {
    SwitchType type;
    SwitchDefaultValue defaultValue;
    unsigned int targetDeviceId;
    unsigned int readingDeviceId;
    DeviceValueUnit deviceUnit;
    DeviceValues targetValue;
    DeviceValues lowValue;
    DeviceValues highValue;
    DeviceValues maxAllowedDifference;
    BasicStackString<MaxArgumentLength> readingArgument;
    BasicStackString<MaxArgumentLength> targetArgument;
};

class SwitchDriver final {
    public:
        static inline constexpr char name[] = "SwitchDriver";

        SwitchDriver(const SwitchDriver &other) = delete;
        SwitchDriver(SwitchDriver &&other) noexcept;
        ~SwitchDriver();

        SwitchDriver &operator=(const SwitchDriver &other) = delete;
        SwitchDriver &operator=(SwitchDriver &&other) noexcept;

        static std::expected<SwitchDriver, const char *> create_driver(const std::string_view input, DeviceConfig &deviceConfOut);
        static std::expected<SwitchDriver, const char *> create_driver(const DeviceConfig *config);

        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value) { return DeviceOperationResult::not_supported; }
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult call_device_action(DeviceConfig *conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data(); 

    private:
        SwitchDriver(const DeviceConfig *conf);

        static void watchValues(std::stop_token token, SwitchDriver *instance);

        const DeviceConfig *mConf;
        std::jthread mWatchValueThread;
};