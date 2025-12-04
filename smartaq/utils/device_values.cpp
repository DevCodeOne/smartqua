#include "utils/device_values.h"

std::optional<DeviceValues> DeviceValues::sum(const DeviceValues& other) const
{
    return operation(other, std::plus<>{});
}

std::optional<DeviceValues> DeviceValues::difference(const DeviceValues& other) const
{
    return operation(other, std::minus<>{});
}
