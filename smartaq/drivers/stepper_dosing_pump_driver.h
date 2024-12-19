#pragma once 

#include <cstdint>
#include <string_view>

#include "build_config.h"
#include "drivers/device_types.h"
#include "build_config.h"

struct StepperDosingConfig {
    unsigned int deviceId = -1;
    int stepsTimesTenPerMl = 0;
    BasicStackString<MaxArgumentLength> writeArgument;
};

// TODO: rename class to better reflect its purpose,
// this class should also be able to dose in time e.g. let the pump run for a specific time frame
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

        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data() { return DeviceOperationResult::not_supported; }

    private:

        StepperDosingPumpDriver(const DeviceConfig *conf);

        const DeviceConfig *mConf;
};