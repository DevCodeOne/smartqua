#pragma once 

#include <cstdint>
#include <string_view>

#include "build_config.h"
#include "drivers/device_types.h"

struct DosingPumpConfig {
    unsigned int deviceId = -1;
    int unitTimesTenPerMl = 0;
    BasicStackString<MaxArgumentLength> writeArgument;
};

// TODO: this class should also be able to dose in time e.g. let the pump run for a specific time frame
class DosingPumpDriver final {
    public:
        static constexpr char name[] = "dosing_pump";

        DosingPumpDriver(const DosingPumpDriver &other) = delete;
        DosingPumpDriver(DosingPumpDriver &&other) = default;
        ~DosingPumpDriver() = default;

        DosingPumpDriver &operator=(const DosingPumpDriver &other) = delete;
        DosingPumpDriver &operator=(DosingPumpDriver &&other) = default;

        static std::optional<DosingPumpDriver> create_driver(const std::string_view input, DeviceConfig&deviceConfOut);
        static std::optional<DosingPumpDriver> create_driver(const DeviceConfig*config);

        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data() { return DeviceOperationResult::not_supported; }

    private:

        DosingPumpDriver(const DeviceConfig *conf);

        const DeviceConfig *mConf;
};