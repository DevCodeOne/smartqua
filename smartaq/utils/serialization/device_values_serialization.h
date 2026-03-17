#pragma once

#include "utils/device_values.h"

#include "build_config.h"

template<>
struct read_from_json<DeviceValues>
{
    static void read(const char* str, int len, DeviceValues& values)
    {

        auto read_and_write_to_optional = [&]<DeviceValueUnit Unit>(std::integral_constant<DeviceValueUnit, Unit>,
                                                                   const char* input,
                                                                   int input_len) -> std::optional<DeviceValues>
        {
            DeviceValueUnitMap::map<Unit> formatDest;
            const auto foundPossibleFormats = std::ranges::find_if(Detail::UnitNames, [](const auto &currentKeyPairSet)
            {
                return currentKeyPairSet.first == Unit;
            });

            if  (foundPossibleFormats == Detail::UnitNames.end())
            {
                return {};
            }
            const auto listOfPossibleFormats = foundPossibleFormats->second;

            return std::visit([&](const auto& realList) -> std::optional<DeviceValues>
            {
                for (const auto& currentFormat : realList)
                {
                    // TODO: generate the correct format
                    auto createdFormat = create_json_format<true, 32>(
                        std::string_view{currentFormat.begin(), currentFormat.end()}, std::optional{formatDest});
                    int result = json_scanf(input, input_len, createdFormat.begin(), &formatDest);
                    if (result > 0)
                    {
                        Logger::log(LogLevel::Info, "Writing to %s", realList.begin()->begin());
                        return DeviceValues::create_from_unit(Unit, formatDest);
                    }
                }
                return {};
            }, listOfPossibleFormats);
        };


        std::optional<DeviceValues> createdValue{};

        constexprFor<DeviceValueUnitList::Size>([&]<size_t Index>(std::integral_constant<size_t, Index>)
        {
            if (not createdValue)
            {
                createdValue = read_and_write_to_optional(
                    std::integral_constant<DeviceValueUnit, DeviceValueUnitList::TypeAt<Index>::value>{}, str,
                    len);
            }
        });

        if (createdValue)
        {
            values = *createdValue;
        } else
        {
            values.invalidate();
        }
    }
};

template<>
struct print_to_json<DeviceValues> {
    static int print(json_out *out, const DeviceValues &values) {
        int written = 0;
        bool has_prev = false;

        auto write_value = [&written, &values, &has_prev, &out]<auto Index>(std::integral_constant<size_t, Index> IndexConstant) {
            constexpr DeviceValueUnit Unit = DeviceValueUnitList::TypeAt<Index>::value;

            // Skip if the unit does not match
            if (values.getUnit() != Unit) {
                return;
            }

            if (values.getUnit() == DeviceValueUnit::none)
            {
                json_printf(out, "{}");
                return;
            }

            // Retrieve the type-safe value using the unit as type
            auto valueOptional = values.getAsUnitAsType<Unit>();
            if (!valueOptional) {
                return;
            }

            // Use the unit name from `Detail::UnitNames` for the key
            const auto foundPossibleFormats = std::ranges::find_if(Detail::UnitNames, [](const auto &currentKeyPairSet)
            {
                return currentKeyPairSet.first == Unit;
            });

            if  (foundPossibleFormats == Detail::UnitNames.end())
            {
                return;
            }

            const auto listOfPossibleFormats = foundPossibleFormats->second;
            const frozen::string unitNameAsFrozen = std::visit(
                [](const auto& currentList) { return *currentList.begin(); }, listOfPossibleFormats);
            const BasicStackString<32> key{unitNameAsFrozen.begin()};

            // Create the format string
            auto format = create_json_format<false, 32>(key.getStringView(), valueOptional);

            if (has_prev) {
                written += json_printf(out, ",");
            }

            // Write the key-value pair
            written += json_printf(out, format.data(), valueOptional.value());

            has_prev = true;
        };

        constexprFor<DeviceValueUnitList::Size>(write_value);

        return written;
    }
};

