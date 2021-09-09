#pragma once

#include <optional>

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "utils/stack_string.h"
#include "smartqua_config.h"

/*

payload : {
    channels : { "b" : 0, "w" : 1 }
}

payload: {
    // Will only be used on this specific day
    {
    day: 0
    schedule: "10-00:r=10,b=5;11-00:r=10,b=10;12-00:r=50,b=40;14-00:r=70,b=70;19-00-r=30,b=50;20-00-r=10,b=20;"
    }
    Without day will be repeated
    {
    schedule: "10-00:r=10,b=5;11-00 r=10,b=10;12:00 r=50,b=40;14:00 r=70,b=70;19:00 r=30,b=50;20:00 r=10,b=20;"
    }
]

*/

struct LampDriverData final {
    std::array<std::optional<stack_string<10>>, 5> channelNames;
    stack_string<name_length> scheduleName;
};

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

    private:
        LampDriver(const device_config *conf);
};