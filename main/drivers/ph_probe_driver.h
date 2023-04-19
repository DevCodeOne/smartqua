#pragma once

#pragma once 

#include <cstdint>
#include <memory>
#include <thread>
#include <string_view>

#include "build_config.h"
#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "smartqua_config.h"

#include "driver/rmt_types.h"

static uint8_t constexpr MaxArgumentLength = 32;

struct PhValuePair {
    uint16_t analogReading;
    float ph;
};

struct PhProbeConfig {
    unsigned int analogDeviceId;
    unsigned int temperatureDeviceId;
    PhValuePair lowerPhPair;
    PhValuePair higherPhPair;
    stack_string<MaxArgumentLength> analogReadingArgument;
    stack_string<MaxArgumentLength> temperatureReadingArgument;
};

class PhProbeDriver final {
    public:
        static inline constexpr char name[] = "PhProbeDriver";

        PhProbeDriver(const PhProbeDriver &other) = delete;
        PhProbeDriver(PhProbeDriver &&other);
        ~PhProbeDriver() = default;

        PhProbeDriver &operator=(const PhProbeDriver &other) = delete;
        PhProbeDriver &operator=(PhProbeDriver &&other);

        static std::optional<PhProbeDriver> create_driver(const std::string_view input, device_config &device_conf_out);
        static std::optional<PhProbeDriver> create_driver(const device_config *config);

        DeviceOperationResult read_value(std::string_view what, device_values &value) const;
        DeviceOperationResult write_value(const device_values &value) { return DeviceOperationResult::not_supported; }
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult call_device_action(device_config *conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data() { return DeviceOperationResult::not_supported; }

    private:
        static void updatePhValue(std::stop_token token, PhProbeDriver *instance);

        PhProbeDriver(const device_config *conf);

        const device_config *mConf;

        std::jthread mPhThread;
};