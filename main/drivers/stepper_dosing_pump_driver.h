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

struct StepperDosingConfig {
    unsigned int deviceId = -1;
    int stepsTimesTenPerMl = 0;
    stack_string<MaxArgumentLength> writeArgument;
};

class StepperDosingPumpDriver final {
    public:
        static inline constexpr char name[] = "dosing_pump";

        StepperDosingPumpDriver(const StepperDosingPumpDriver &other) = delete;
        StepperDosingPumpDriver(StepperDosingPumpDriver &&other) = default;
        ~StepperDosingPumpDriver() = default;

        StepperDosingPumpDriver &operator=(const StepperDosingPumpDriver &other) = delete;
        StepperDosingPumpDriver &operator=(StepperDosingPumpDriver &&other) = default;

        static std::optional<StepperDosingPumpDriver> create_driver(const std::string_view input, DeviceConfig&device_conf_out);
        static std::optional<StepperDosingPumpDriver> create_driver(const DeviceConfig*config);

        DeviceOperationResult read_value(std::string_view what, device_values &value) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult write_value(std::string_view what, const device_values &value);
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data() { return DeviceOperationResult::not_supported; }

    private:

        StepperDosingPumpDriver(const DeviceConfig *conf);

        const DeviceConfig *mConf;
};