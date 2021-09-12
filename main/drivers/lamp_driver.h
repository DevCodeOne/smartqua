#pragma once

#include <array>
#include <optional>
#include <cstdint>
#include <utility>

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "utils/stack_string.h"
#include "utils/schedule.h"
#include "utils/json_utils.h"
#include "utils/sd_filesystem.h"
#include "smartqua_config.h"

// TODO: make configurable
static inline constexpr uint8_t NumChannels = 4;
static inline constexpr uint8_t MaxChannelNameLength = 4;
static inline constexpr uint8_t TimePointsPerDay = 12;

// TODO: consider own datatype for channelNames + deviceIndices
struct LampDriverData final {
    std::array<std::optional<stack_string<MaxChannelNameLength>>, NumChannels> channelNames;
    std::array<std::optional<int>, NumChannels> deviceIndices;
    stack_string<name_length * 2> scheduleName;
};

// TODO: Create percentage class, static_lookuptable
struct LampTimePoint {
    std::array<std::optional<uint8_t>, NumChannels> percentages;
};

// TODO: implement
/*
payload : {
    channels : { "b" : 0, "w" : 1 }
    schedule: {
        // Will only be used on this specific day
        "monday" : "10-00:r=10,b=5;11-00:r=10,b=10;12-00:r=50,b=40;14-00:r=70,b=70;19-00-r=30,b=50;20-00-r=10,b=20;"
        "repeating" : "10-00:r=10,b=5;11-00 r=10,b=10;12:00 r=50,b=40;14:00 r=70,b=70;19:00 r=30,b=50;20:00 r=10,b=20;"
    }
*/

class LampDriver final {
    public:
        using ScheduleType = WeekSchedule<LampTimePoint, 12>;

        static inline constexpr char name[] = "lamp_driver";
        static inline constexpr char path_format[] = "%s/schedules/lampdriver/%d-%d.json";

        LampDriver(const LampDriver &other) = delete;
        LampDriver(LampDriver &&other) = default;
        ~LampDriver() = default;

        LampDriver &operator=(const LampDriver &other) = delete;
        LampDriver &operator=(LampDriver &&other) = default;

        static std::optional<LampDriver> create_driver(const std::string_view &input, device_config &device_conf_out);
        static std::optional<LampDriver> create_driver(const device_config *config);

        device_operation_result write_value(const device_values &value);
        device_operation_result read_value(device_values &value) const;
        device_operation_result get_info(char *output, size_t output_buffer_len) const;
        device_operation_result write_device_options(const char *json_input, size_t input_len);
        device_operation_result update_runtime_data();

        std::optional<uint8_t> channelIndex(std::string_view channelName) const;

    private:
        bool loadAndUpdateSchedule(const std::string_view &input);

        LampDriver(const device_config *conf);

        const device_config *mConf;
        ScheduleType schedule;

        friend struct read_from_json<LampDriver>;
};

std::optional<LampDriver::ScheduleType::DayScheduleType> parseDaySchedule(const LampDriver &driver, const std::string_view &strValue);

template<>
struct read_from_json<LampDriver> {
    static void read(const char *str, int len, LampDriver &userData) {
        std::array<json_token, 8> dayTokens{};
        json_scanf(str, len, "{ mon : %T, tue : %T, wed : %T, thu : %T, fri : %T, sat : %T, sun : %T, repeating : %T }",
            &dayTokens[static_cast<uint32_t>(weekday::monday)],
            &dayTokens[static_cast<uint32_t>(weekday::tuesday)],
            &dayTokens[static_cast<uint32_t>(weekday::wednesday)],
            &dayTokens[static_cast<uint32_t>(weekday::thursday)],
            &dayTokens[static_cast<uint32_t>(weekday::friday)],
            &dayTokens[static_cast<uint32_t>(weekday::saturday)],
            &dayTokens[static_cast<uint32_t>(weekday::sunday)],
            &dayTokens[7]);

        // Maybe allow mixture between repeating and other days, wich override the repeating value
        if (dayTokens[7].len > 0) {
            ESP_LOGI("LampDriver", "Using repeating schedule value : %.*s", dayTokens[7].len, dayTokens[7].ptr);
            auto result = parseDaySchedule(userData, std::string_view(dayTokens[7].ptr, dayTokens[7].len));

            if (!result.has_value()) {
                ESP_LOGI("LampDriver", "Couldn't parse daily info");
            }

            for (uint8_t i = 0; i < 7; ++i) {
                userData.schedule.setDaySchedule(weekday(i), *result);
            }
            return;
        }
        ESP_LOGI("LampDriver", "Repeating wasn't set");
    }
};