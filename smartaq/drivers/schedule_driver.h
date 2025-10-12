#pragma once

#include <array>
#include <optional>
#include <cstdint>
#include <chrono>
#include <string_view>

#include "build_config.h"
#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "drivers/devices.h"
#include "utils/stack_string.h"
#include "utils/logger.h"
#include "utils/serialization/json_utils.h"
#include "utils/time/schedule.h"
#include "utils/time/schedule_tracker.h"

static inline constexpr auto MaxLocalPathLength = 24;

// TODO: consider own datatype for channelNames + deviceIndices
struct ScheduleDriverData final {
    std::array<BasicStackString<schedule_max_channel_name_length>, schedule_max_num_channels> channelNames;
    // TODO: change this to uint8_t later on
    std::array<std::optional<int>, schedule_max_num_channels> deviceIndices;
    std::array<BasicStackString<schedule_max_channel_name_length>, schedule_max_num_channels> deviceArguments;
    std::array<DeviceValueUnit, schedule_max_num_channels> channelUnit{};
    BasicStackString<MaxLocalPathLength> schedulePath;
    BasicStackString<MaxLocalPathLength> scheduleStatePath;
    ScheduleEventTransitionMode type;
    uint16_t creationId = 0;
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
        using ScheduleTrackerType = ScheduleTracker<ScheduleType, float, schedule_max_num_channels>;
        using NewChannelValues = ScheduleTrackerType::OptionalChannelValues;

        static constexpr char name[] = "schedule_driver";
        static constexpr char schedulePathFormat[] = "%s/%d.json";
        static constexpr char scheduleStatePathFormat[] = "%s/%d.state";

        ScheduleDriver(const ScheduleDriver &other) = delete;
        ScheduleDriver(ScheduleDriver &&other) noexcept;
        ~ScheduleDriver() = default;

        ScheduleDriver &operator=(const ScheduleDriver &other) = delete;
        ScheduleDriver &operator=(ScheduleDriver &&other) noexcept;

        static std::optional<ScheduleDriver> create_driver(const std::string_view &input, DeviceConfig &deviceConfOut);
        static std::optional<ScheduleDriver> create_driver(const DeviceConfig *config);

        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data();

        [[nodiscard]] std::optional<uint8_t> channelIndex(std::string_view channelName) const;

    private:
        explicit ScheduleDriver(const DeviceConfig *conf);

        DeviceOperationResult updateValues();
        bool loadAndUpdateSchedule(const std::string_view &input);
        bool readChannelTimes();
        [[nodiscard]] bool synchronizeChannelTimesToFile() const;

        const DeviceConfig *mConf;
        ScheduleType schedule;
        ScheduleTrackerType scheduleTracker;

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
struct read_from_json<ScheduleEventTransitionMode> {
    static void read(const char *str, int len, ScheduleEventTransitionMode &driverType) {
        if (str == nullptr || len == 0) {
            return;
        }

        std::string_view as_view{str, static_cast<size_t>(len)};

        Logger::log(LogLevel::Debug, "Reading DriverType %.*s", as_view.size(), as_view.data());

        if (as_view == "action") {
            driverType = ScheduleEventTransitionMode::SingleShot;
        } else if (as_view == "action_hold") {
            driverType = ScheduleEventTransitionMode::Hold;
        } else if (as_view == "interpolate") {
            driverType = ScheduleEventTransitionMode::Interpolation;
        }
    }
};

// TODO: replace, this doesn't make sense, since it doesn't actually initialize the complete ScheduleDriver
template<>
struct read_from_json<ScheduleDriver> {
    static void read(const char *str, int len, ScheduleDriver &userData) {
        std::array<json_token, 8> dayTokens{};
        json_scanf(str, len, "{ mon : %T, tue : %T, wed : %T, thu : %T, fri : %T, sat : %T, sun : %T, repeating : %T }",
            &dayTokens[static_cast<uint32_t>(WeekDay::monday)],
            &dayTokens[static_cast<uint32_t>(WeekDay::tuesday)],
            &dayTokens[static_cast<uint32_t>(WeekDay::wednesday)],
            &dayTokens[static_cast<uint32_t>(WeekDay::thursday)],
            &dayTokens[static_cast<uint32_t>(WeekDay::friday)],
            &dayTokens[static_cast<uint32_t>(WeekDay::saturday)],
            &dayTokens[static_cast<uint32_t>(WeekDay::sunday)],
            &dayTokens[7]);

        // Maybe allow mixture between repeating and other days, which override the repeating value
        if (dayTokens[7].len > 0) {
            Logger::log(LogLevel::Info, "Using repeating schedule value : %.*s", dayTokens[7].len, dayTokens[7].ptr);
            auto result = parseDaySchedule(userData, std::string_view(dayTokens[7].ptr, dayTokens[7].len));

            if (!result.has_value()) {
                Logger::log(LogLevel::Warning, "Couldn't parse daily info");
            }

            for (uint8_t i = 0; i < 7; ++i) {
                userData.schedule.setDaySchedule(WeekDay(i), *result);
            }
            return;
        }
        // TODO: implement other days
        Logger::log(LogLevel::Warning, "Repeating wasn't set");
    }
};