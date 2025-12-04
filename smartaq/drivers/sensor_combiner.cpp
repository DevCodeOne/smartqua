#include "sensor_combiner.h"

#include <numeric>

#include "actions/device_actions.h"

int SensorCombiner::readDeviceValues(SensorCombinerData* config,
                                     std::array<DeviceValues, SensorCombinerData::MaxDevices> values)
{
    int readValues = 0;
    for (int i = 0; i < SensorCombinerData::MaxDevices; i++)
    {
        if (!config->deviceIndices[i].has_value())
        {
            continue;
        }

        auto result = readDeviceValue(static_cast<unsigned int>(*config->deviceIndices[i]),
                                      config->deviceArguments[i].getStringView());

        if (!result.has_value())
        {
            continue;
        }

        values[readValues++] = *result;
    }

    return readValues;
}
std::expected<SensorCombiner, std::string_view> SensorCombiner::create_driver(const std::string_view& input,
                                                                              DeviceConfig& deviceConfOut)
{
    SensorCombinerData newConf{};
    newConf.strategy = SensorCombinationStrategy::Average;

    json_scanf(input.data(), input.size(),
               "{ device_indices: %M, device_arguments: %M, at_least_valid: %u, strategy: %M }",
               json_scanf_array<decltype(newConf.deviceIndices)>, &newConf.deviceIndices,
                json_scanf_array<decltype(newConf.deviceArguments)>, &newConf.deviceArguments,
               &newConf.atLeastValid,
               json_scanf_single<SensorCombinationStrategy>, &newConf.strategy);

    if (newConf.atLeastValid < 1 || newConf.atLeastValid > SensorCombinerData::MaxDevices)
    {
        return std::unexpected("Invalid atLeastValid value, has to be in range");
    }

    deviceConfOut.insertConfig(&newConf);

    return SensorCombiner{&deviceConfOut};
}

std::expected<SensorCombiner, std::string_view> SensorCombiner::create_driver(const DeviceConfig* deviceConfOut)
{
    return SensorCombiner{deviceConfOut};
}

DeviceOperationResult SensorCombiner::read_value(std::string_view what, DeviceValues& value) const
{
    auto config = mConf->accessConfig<SensorCombinerData>();
    std::array<DeviceValues, SensorCombinerData::MaxDevices> values{};

    value.invalidate();

    const auto readValues = readDeviceValues(config, values);

    if (readValues < config->atLeastValid)
    {
        return DeviceOperationResult::failure;
    }

    auto firstUnit = values[0].getUnit();
    const auto allSameUnit = std::all_of(values.begin(), values.begin() + readValues, [firstUnit](const DeviceValues& v)
    {
        return v.getUnit() == firstUnit;
    });

    if (!allSameUnit)
    {
        return DeviceOperationResult::failure;
    }

    std::array<float, SensorCombinerData::MaxDevices> convertedValues{};

    convertToFloats(readValues, values, convertedValues);

    value = combine(readValues, firstUnit, convertedValues);

    return value.getUnit() != DeviceValueUnit::none ? DeviceOperationResult::ok : DeviceOperationResult::failure;
}

void SensorCombiner::convertToFloats(int readValues, const std::array<DeviceValues, SensorCombinerData::MaxDevices> &valuesToConvert, std::array<float, SensorCombinerData::MaxDevices> &values)
{
    for (int i = 0; i < SensorCombinerData::MaxDevices && i < readValues; i++)
    {
        values[i] = valuesToConvert[i].getAsUnit<float, true>().value_or(0.0f);
    }
}

SensorCombiner::SensorCombiner(const DeviceConfig* config) : mConf(config)
{
}

DeviceValues SensorCombiner::combine(int readValues, DeviceValueUnit unit,
                                     const std::array<float, SensorCombinerData::MaxDevices>& values) const
{
    const auto config = mConf->accessConfig<SensorCombinerData>();

    if (config->strategy == SensorCombinationStrategy::Average && readValues > 0)
    {
        const auto average = std::accumulate(values.begin(), values.begin() + readValues, 0.0f) / static_cast<float>(readValues);
        return DeviceValues::create_from_unit(unit, average);
    }

    return DeviceValues{};
}
