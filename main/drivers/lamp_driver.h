#pragma once

#include <array>
#include <optional>
#include <cstdint>
#include <utility>
#include <chrono>
#include <string_view>

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "utils/stack_string.h"
#include "utils/schedule.h"
#include "utils/json_utils.h"
#include "utils/sd_filesystem.h"
#include "utils/logger.h"
#include "storage/rest_storage.h"
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

// TODO: Add possibillity to use more than one schedule
struct RestSchedule {
    static inline constexpr char name[] = "RemoteScheduleData";

    template<size_t ArraySize>
    static bool generateRestTarget(std::array<char, ArraySize> &dst) {
        std::array<uint8_t, 6> mac;
        esp_efuse_mac_get_default(mac.data());
        snprintf(dst.data(), dst.size(), "http://%s/values/%02x-%02x-%02x-%02x-%02x-%02x-%s", 
            remote_setting_host,
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            name);

        // TODO: check return value
        return true;
    }
};

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

        DeviceOperationResult write_value(const device_values &value);
        DeviceOperationResult read_value(device_values &value) const;
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult write_device_options(const char *json_input, size_t input_len);
        DeviceOperationResult update_runtime_data();
        DeviceOperationResult update_values(const std::array<std::optional<int>, NumChannels> &channel_data);
        std::array<std::optional<int>, NumChannels> retrieveCurrentValues();

        std::optional<uint8_t> channelIndex(std::string_view channelName) const;

    private:
        LampDriver(const device_config *conf);

        bool loadAndUpdateSchedule(const std::string_view &input);

        const device_config *mConf;
        ScheduleType schedule;
        RestStorage<RestSchedule, RestDataType::Json> remoteSchedule;

        friend struct read_from_json<LampDriver>;
};

std::optional<LampDriver::ScheduleType::DayScheduleType> parseDaySchedule(const LampDriver &driver, const std::string_view &strValue);

/*
payload : {
    channels : { "b" : 0, "w" : 1 },
    schedule: {
        // Will only be used on this specific day
        "monday" : "10-00:r=10,b=5;11-00:r=10,b=10;12-00:r=50,b=40;14-00:r=70,b=70;19-00-r=30,b=50;20-00-r=10,b=20;"
        "repeating" : "10-00:r=10,b=5;11-00 r=10,b=10;12:00 r=50,b=40;14:00 r=70,b=70;19:00 r=30,b=50;20:00 r=10,b=20;"
    }
*/
// TODO: replace, this doesn't make sense, since it doesn't actually initalize the complete LampDriver
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