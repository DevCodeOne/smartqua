#pragma once

#include <array>
#include <optional>
#include <cstdint>
#include <utility>
#include <chrono>
#include <string_view>

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "drivers/devices.h"
#include "utils/stack_string.h"
#include "utils/json_utils.h"
#include "utils/logger.h"
#include "utils/time/schedule.h"
#include "build_config.h"

enum struct DriverType {
    Interpolate, Action, ActionHold
};

static inline constexpr auto MaxLocalPathLength = 24;

// TODO: consider own datatype for channelNames + deviceIndices
struct ScheduleDriverData final {
    std::array<std::optional<BasicStackString<schedule_max_channel_name_length>>, schedule_max_num_channels> channelNames;
    // TODO: change this to uint8_t later on
    std::array<std::optional<int>, schedule_max_num_channels> deviceIndices;
    std::array<DeviceValueUnit, schedule_max_num_channels> channelUnit{};
    BasicStackString<MaxLocalPathLength> schedulePath;
    BasicStackString<MaxLocalPathLength> scheduleStatePath;
    DriverType type;
    uint16_t creationId = 0;
};

struct ScheduleState {
    std::array<std::chrono::seconds, schedule_max_num_channels> channelTime{};
};

/*
class RestSchedule {
    public:
        RestSchedule(const char *name, uint16_t creationId) : mName(name), mCreationId(creationId) { }

        template<size_t ArraySize>
        bool generateRestTarget(std::array<char, ArraySize> &dst) {
            std::array<uint8_t, 6> mac;
            esp_efuse_mac_get_default(mac.data());
            snprintf(dst.data(), dst.size(), "http://%s/values/%02x-%02x-%02x-%02x-%02x-%02x-%u-%s", 
                remote_setting_host,
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                mCreationId,
                mName);

            // TODO: check return value
            return true;
        }

    private:
        const char *mName = nullptr;
        uint16_t mCreationId = 0;
};
*/

class ScheduleDriver final {
    public:
        using ScheduleType = WeekSchedule<schedule_max_num_channels, float, 12>;
        using TimePointValueData = std::tuple<int, std::chrono::seconds, float>;
        using ChannelData = ScheduleType::DayScheduleType::ChannelData;
        using NewChannelValues = std::array<std::optional<std::tuple<int, std::chrono::seconds, float>>, schedule_max_num_channels>;

        static constexpr char name[] = "schedule_driver";
        static constexpr char schedulePathFormat[] = "%s/%d.json";
        static constexpr char scheduleStatePathFormat[] = "%s/%d.state";

        ScheduleDriver(const ScheduleDriver &other) = delete;
        ScheduleDriver(ScheduleDriver &&other) = default;
        ~ScheduleDriver() = default;

        ScheduleDriver &operator=(const ScheduleDriver &other) = delete;
        ScheduleDriver &operator=(ScheduleDriver &&other) = default;

        static std::optional<ScheduleDriver> create_driver(const std::string_view &input, DeviceConfig&device_conf_out);
        static std::optional<ScheduleDriver> create_driver(const DeviceConfig*config);

        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data();
        DeviceOperationResult updateValues(const NewChannelValues &channel_data);

        std::optional<uint8_t> channelIndex(std::string_view channelName) const;

    private:
        ScheduleDriver(const DeviceConfig*conf);

        bool loadAndUpdateSchedule(const std::string_view &input);
        bool updateScheduleState(const NewChannelValues &values);
        NewChannelValues retrieveCurrentValues();
        NewChannelValues interpolateValues(ScheduleDriverData *scheduleDriverConf);
        NewChannelValues simpleValues(ScheduleDriverData *scheduleDriverConf);

        const DeviceConfig *mConf;
        ScheduleType schedule;
        ScheduleState scheduleState;

        friend struct read_from_json<ScheduleDriver>;
};

std::optional<ScheduleDriver::ScheduleType::DayScheduleType> parseDaySchedule(const ScheduleDriver &driver, const std::string_view &strValue);

/*
payload : {
    channels : { "b" : 0, "w" : 1 },
    schedule: {
        // Will only be used on this specific day
        "monday" : "10-00:r=10,b=5;11-00:r=10,b=10;12-00:r=50,b=40;14-00:r=70,b=70;19-00-r=30,b=50;20-00-r=10,b=20;"
        "repeating" : "10-00:r=10,b=5;11-00 r=10,b=10;12:00 r=50,b=40;14:00 r=70,b=70;19:00 r=30,b=50;20:00 r=10,b=20;"
    }
*/
template<>
struct read_from_json<DriverType> {
    static void read(const char *str, int len, DriverType &driverType) {
        if (str != nullptr && len > 0) {
            std::string_view as_view{str, static_cast<size_t>(len)};

            Logger::log(LogLevel::Debug, "Reading DriverType %.*s", as_view.size(), as_view.data());

            if (as_view == "action") {
                driverType = DriverType::Action;
            } else if (as_view == "action_hold") { 
                driverType = DriverType::ActionHold;
            } else if (as_view == "interpolate") {
                driverType = DriverType::Interpolate;
            }
        }
    }
};

// TODO: replace, this doesn't make sense, since it doesn't actually initalize the complete ScheduleDriver
template<>
struct read_from_json<ScheduleDriver> {
    static void read(const char *str, int len, ScheduleDriver &userData) {
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

        // Maybe allow mixture between repeating and other days, which override the repeating value
        if (dayTokens[7].len > 0) {
            Logger::log(LogLevel::Info, "Using repeating schedule value : %.*s", dayTokens[7].len, dayTokens[7].ptr);
            auto result = parseDaySchedule(userData, std::string_view(dayTokens[7].ptr, dayTokens[7].len));

            if (!result.has_value()) {
                Logger::log(LogLevel::Warning, "Couldn't parse daily info");
            }

            for (uint8_t i = 0; i < 7; ++i) {
                userData.schedule.setDaySchedule(weekday(i), *result);
            }
            return;
        }
        // TODO: implement other days
        Logger::log(LogLevel::Warning, "Repeating wasn't set");
    }
};