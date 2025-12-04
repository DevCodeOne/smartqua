#pragma once
#include <optional>
#include <expected>

#include "device_types.h"
#include "hal/device_config.h"
#include "utils/stack_string.h"

enum struct SensorCombinationStrategy
{
    Average
};

// TODO: Create own variables instead of using schedule... values
struct SensorCombinerData final
{
    static constexpr size_t MaxDevices = schedule_max_num_channels;

    std::array<std::optional<int>, schedule_max_num_channels> deviceIndices;
    std::array<BasicStackString<schedule_max_channel_name_length>, schedule_max_num_channels> deviceArguments;
    SensorCombinationStrategy strategy;
    unsigned int atLeastValid;
};

class SensorCombiner
{
public:
    static constexpr char name[] = "sensor_combiner";

    static std::expected<SensorCombiner, std::string_view> create_driver(const std::string_view &input, DeviceConfig &deviceConfOut);
    static std::expected<SensorCombiner, std::string_view> create_driver(const DeviceConfig *config);

    DeviceOperationResult write_value(std::string_view, const DeviceValues& ) { return DeviceOperationResult::not_supported; }
    DeviceOperationResult read_value(std::string_view what, DeviceValues& value) const;
    DeviceOperationResult get_info(char* output, size_t output_buffer_len) const { return DeviceOperationResult::not_supported; }
    DeviceOperationResult call_device_action(DeviceConfig* conf, const std::string_view& action,
                                             const std::string_view& json) { return DeviceOperationResult::not_supported; }
    DeviceOperationResult update_runtime_data() { return DeviceOperationResult::ok; }
private:
    SensorCombiner(const DeviceConfig* config);

    [[nodiscard]] DeviceValues combine(int readValues, DeviceValueUnit unit, const std::array<float, SensorCombinerData::MaxDevices> &values) const;
    static void convertToFloats(int readValues, const std::array<DeviceValues, SensorCombinerData::MaxDevices> &valuesToConvert, std::array<float, SensorCombinerData::MaxDevices> &values);
    static int readDeviceValues(SensorCombinerData* config,
                                std::array<DeviceValues, SensorCombinerData::MaxDevices> values);

    const DeviceConfig* mConf;
};

template<>
struct read_from_json<SensorCombinationStrategy> {
    static void read(const char *str, int len, SensorCombinationStrategy &driverType) {
        if (str == nullptr || len == 0) {
            return;
        }

        std::string_view as_view{str, static_cast<size_t>(len)};

        if (as_view == "average") {
            driverType = SensorCombinationStrategy::Average;
        }
    }
};

