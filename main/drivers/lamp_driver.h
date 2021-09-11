#pragma once

#include <array>
#include <optional>

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "utils/stack_string.h"
#include "utils/schedule.h"
#include "utils/time_utils.h"
#include "smartqua_config.h"

// TODO: make configurable
static inline constexpr auto NumChannels = 4;
static inline constexpr auto MaxChannelNameLength = 10;
static inline constexpr auto TimePointsPerDay = 12;

struct LampDriverData final {
    std::array<std::optional<stack_string<MaxChannelNameLength>>, NumChannels> channelNames;
    stack_string<name_length> scheduleName;
};

// TODO: Create percentage class
struct ScheduleTimePoint {
    std::chrono::seconds secondsThatDay;
    std::array<std::optional<uint8_t>, NumChannels> percentages;
};

/*

payload : {
    channels : { "b" : 0, "w" : 1 }
}

payload: {
    // Will only be used on this specific day
    "monday" : 
    {
    "schedule": "10-00:r=10,b=5;11-00:r=10,b=10;12-00:r=50,b=40;14-00:r=70,b=70;19-00-r=30,b=50;20-00-r=10,b=20;"
    }
    "repeating" :
    {
    schedule: "10-00:r=10,b=5;11-00 r=10,b=10;12:00 r=50,b=40;14:00 r=70,b=70;19:00 r=30,b=50;20:00 r=10,b=20;"
    }
]

*/

class LampDriver final {
    public:
        static inline constexpr char name[] = "lamp_driver";
        static inline constexpr char schedule_regexp[]  = R"((?<hours>[0-1][\d]|[2][0-3])-(?<mins>[0-5][0-9]):(?<vars>(?:\w{0,10}=\d*,?)*);?)";
        static inline constexpr char varextraction_regexp[] = R"((?:(?<variable>\w{0,10})=(?<value>\d*),?))";

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

    private:
        bool write_schedule(const std::string_view &payload);
        bool read_todays_schedule(weekday currentDay);

        LampDriver(const device_config *conf);

        const device_config *mConf;
        WeekSchedule<ScheduleTimePoint, 12> schedule;
};