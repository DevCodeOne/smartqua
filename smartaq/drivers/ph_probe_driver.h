#pragma once 

#include <cstdint>
#include <memory>
#include <thread>
#include <string_view>

#include "build_config.h"
#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "build_config.h"

#include "driver/rmt_types.h"

struct PhValuePair {
    uint16_t analogReading;
    float ph;
};

struct PhProbeConfig {
    unsigned int analogDeviceId;
    unsigned int temperatureDeviceId;
    PhValuePair lowerPhPair;
    PhValuePair higherPhPair;
    BasicStackString<MaxArgumentLength> analogReadingArgument;
    BasicStackString<MaxArgumentLength> temperatureReadingArgument;
};

class PhProbeDriver final {
    public:
        static inline constexpr char name[] = "PhProbeDriver";

        PhProbeDriver(const PhProbeDriver &other) = delete;
        PhProbeDriver(PhProbeDriver &&other) noexcept;
        ~PhProbeDriver() = default;

        PhProbeDriver &operator=(const PhProbeDriver &other) = delete;
        PhProbeDriver &operator=(PhProbeDriver &&other) noexcept;

        static std::optional<PhProbeDriver> create_driver(const std::string_view input, DeviceConfig&deviceConfOut);
        static std::optional<PhProbeDriver> create_driver(const DeviceConfig*config);

        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value) { return DeviceOperationResult::not_supported; }
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data() { return DeviceOperationResult::not_supported; }

    private:
        static void updatePhValue(std::stop_token token, PhProbeDriver *instance);

        PhProbeDriver(const DeviceConfig*conf);

        const DeviceConfig*mConf;

        std::jthread mPhThread;
};