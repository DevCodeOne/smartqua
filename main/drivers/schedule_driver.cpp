#include "schedule_driver.h"

#include <charconv>
#include <chrono>

#include "ctre.hpp"

#include "drivers/device_types.h"
#include "actions/device_actions.h"
#include "frozen.h"
#include "utils/filesystem_utils.h"
#include "utils/json_utils.h"
#include "utils/sd_filesystem.h"
#include "utils/idf_utils.h"
#include "utils/logger.h"
#include "aq_main.h"
#include "utils/time_utils.h"

// TODO: maybe fix target value range 0-100
static constexpr inline ctll::fixed_string schedule_regexp{R"((?<hours>[0-1][\d]|[2][0-3])-(?<mins>[0-5][0-9]):(?<vars>(?:\w{0,10}=\d*,?)*);?)"};
static constexpr inline ctll::fixed_string varextraction_regexp{R"((?:(?<variable>\w{0,10})=(?<value>\d*),?))"};

static constexpr inline ctll::fixed_string hours_name{"hours"};
static constexpr inline ctll::fixed_string mins_name{"mins"};
static constexpr inline ctll::fixed_string vars_name{"vars"};
static constexpr inline ctll::fixed_string variable_name{"variable"};
static constexpr inline ctll::fixed_string value_name{"value"};

std::optional<ScheduleDriver::ScheduleType::DayScheduleType> parseDaySchedule(const ScheduleDriver &driver, const std::string_view &strValue) {
    ScheduleDriver::ScheduleType::DayScheduleType generatedSchedule;
    bool containsData = false;

    for (auto timePointMatch : ctre::range<schedule_regexp>(strValue)) {

        containsData = false;

        const auto hoursView = timePointMatch.get<hours_name>().to_view();
        const auto minsView = timePointMatch.get<mins_name>().to_view();

        uint32_t hoursValue = 0;
        uint32_t minsValue = 0;
        std::from_chars(hoursView.data(), hoursView.data() + hoursView.size(), hoursValue);
        std::from_chars(minsView.data(), minsView.data() + minsView.size(), minsValue);
        const std::chrono::seconds timeOfDay = std::chrono::hours(hoursValue) + std::chrono::minutes(minsValue);

        PercentageTimePoint timePointData{};
        for (auto variableMatch : ctre::range<varextraction_regexp>(timePointMatch.get<vars_name>())) {
            const auto variableView = variableMatch.get<variable_name>().to_view();
            const auto valueView = variableMatch.get<value_name>().to_view();
            uint8_t valueAsInt = 0;

            std::from_chars(valueView.data(), valueView.data() + valueView.size(), valueAsInt);

            // TODO: if this fails, should the whole op fail ?
            if (auto channelIndex = driver.channelIndex(variableView)) {
                timePointData.percentages[*channelIndex] = valueAsInt;
            } else {
                Logger::log(LogLevel::Warning, "Couldn't find channel %.*s", variableView.length(), variableView.data());
            }

            containsData = true;
        }

        if (containsData) {
            Logger::log(LogLevel::Info, "Inserting Timepoint into schedule");
            generatedSchedule.insertTimePoint(std::make_pair(timeOfDay, timePointData));
        } else {
            const auto varsView = timePointMatch.get<vars_name>().to_view();
            Logger::log(LogLevel::Warning, "%.*s didn't cointain any parseable data", 
                varsView.length(), varsView.data());
        }

    }

    if (!containsData) {
        return std::nullopt;
    }

    return generatedSchedule;
}

std::optional<ScheduleDriver> ScheduleDriver::create_driver(const std::string_view &input, device_config &device_conf_out) {
    // Only use temporary variable to initialize the real storage
    auto createdConf = reinterpret_cast<ScheduleDriverData *>(device_conf_out.device_config.data());
    {
        ScheduleDriverData newConf{};

        // TODO: type action should also support action_hold, instead of single shot for dosing pumps, e.g. co2 magnetic valves
        // TODO: Channel names should be equivalent to the actual device_names
        // TODO: maybe do channel_desc, instead of the individual arrays
        json_scanf(input.data(), input.size(), "{ type : %M, channel_names : %M, devices : %M, channel_units : %M }", 
        json_scanf_single<DriverType>, &newConf.type,
        json_scanf_array<decltype(newConf.channelNames)>, &newConf.channelNames,
        json_scanf_array<decltype(newConf.deviceIndices)>, &newConf.deviceIndices,
        json_scanf_array<decltype(newConf.channelUnit)>, &newConf.channelUnit);

        for (const auto &channelName : newConf.channelNames) {
            Logger::log(LogLevel::Info, "%.*s", channelName->len(), channelName->data());
        }

        bool missingData = false;
        for (uint8_t i = 0; i < NumChannels; ++i) {
            if (newConf.channelNames[i].has_value() != newConf.deviceIndices[i].has_value()) {
                missingData = true;
                break;
            }
        }

        if (missingData) {
            return std::nullopt;
        }

        std::memcpy(reinterpret_cast<ScheduleDriverData *>(device_conf_out.device_config.data()), &newConf, sizeof(ScheduleDriverData));
    }
    
    std::tm timeinfo;
    std::time_t now;

    wait_for_clock_sync(&now, &timeinfo);

    // Seconds since year started, 
    // Yes: collision if one manages to use the same second to create a schedule in a different year
    createdConf->creationId = static_cast<int>(sinceYearBeginning<std::chrono::seconds>().count());

    auto result = snprintf(createdConf->scheduleName.data(), decltype(createdConf->scheduleName)::ArrayCapacity, 
        path_format, sd_filesystem::mount_point, createdConf->creationId);

    if (result < 0) {
        Logger::log(LogLevel::Warning, "Path to write the schedule to was too long");
        return std::nullopt;
    }
    
    ScheduleDriver driver(&device_conf_out);
    if (!driver.loadAndUpdateSchedule(input)) {
        Logger::log(LogLevel::Warning, "Couldn't load and update schedule");
        return std::nullopt;
    }

    return driver;
}

std::optional<ScheduleDriver> ScheduleDriver::create_driver(const device_config *device_conf_out) {
    if (device_conf_out == nullptr) {
        return std::nullopt;
    }

    auto createdConf = reinterpret_cast<ScheduleDriverData *>(device_conf_out->device_config.data());
    auto buffer = LargeBufferPoolType::get_free_buffer();

    if (!buffer.has_value()) {
        Logger::log(LogLevel::Warning, "Couldn't get a free buffer");
        return std::nullopt;
    }

    ScheduleDriver driver(device_conf_out);
    auto result = loadFileCompletelyIntoBuffer(createdConf->scheduleName, buffer->data(), buffer->size());

    if (result <= 0) {
        result = driver.remoteSchedule.retrieveData(buffer->data(), buffer->size());
    }

    if (result <= 0) {
        Logger::log(LogLevel::Warning, "Couldn't open schedule file %s", createdConf->scheduleName.data());
        return std::nullopt;
    }

    std::string_view bufferView(buffer->data(), safe_strlen(buffer->data(), buffer->size()));

    Logger::log(LogLevel::Info, "Parsing schedule %*s", (int) bufferView.length(), bufferView.data());

    if (!driver.loadAndUpdateSchedule(bufferView)) {
        return std::nullopt;
    }

    return driver;
}

bool ScheduleDriver::loadAndUpdateSchedule(const std::string_view &input) {
    auto createdConf = reinterpret_cast<const ScheduleDriverData *>(mConf->device_config.data());
    json_scanf(input.data(), input.size(), "{ schedule : %M }", json_scanf_single<ScheduleDriver>, this);
    Logger::log(LogLevel::Info, "Writing schedule to %s", createdConf->scheduleName.data());

    // TODO: maybe add Workaround for this
    if (!(safeWriteToFile(createdConf->scheduleName, ".tmp", input) || remoteSchedule.writeData(input.data(), input.size()))) {
        Logger::log(LogLevel::Warning, "Failed to write schedule");
        return false;
    }

    ScheduleState newState;
    if (!remoteScheduleState.retrieveData(reinterpret_cast<char *>(&newState), sizeof(ScheduleState))) {
        Logger::log(LogLevel::Warning, "Failed to read remote schedule state");
    }

    scheduleState = newState;
    return true;
}

std::optional<uint8_t> ScheduleDriver::channelIndex(std::string_view channelName) const {
    if (mConf == nullptr) {
        return std::nullopt;
    }

    auto lampDriverConf = reinterpret_cast<const ScheduleDriverData *>(mConf->device_config.data());

    for (uint8_t i = 0; i < NumChannels; ++i) {
        if (!lampDriverConf->channelNames[i].has_value()) {
            continue;
        }

        if (lampDriverConf->channelNames[i]->getStringView() == channelName) {
            return i;
        }
    }
    return std::nullopt;
}

ScheduleDriver::ScheduleDriver(const device_config *conf) : mConf(conf), 
        remoteSchedule("schedule", reinterpret_cast<const ScheduleDriverData *>(mConf->device_config.data())->creationId),
        remoteScheduleState("sState", reinterpret_cast<const ScheduleDriverData *>(mConf->device_config.data())->creationId) {}

DeviceOperationResult ScheduleDriver::write_value(const device_values &value) {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult ScheduleDriver::read_value(device_values &value) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult ScheduleDriver::get_info(char *output, size_t output_buffer_len) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult ScheduleDriver::call_device_action(device_config *conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult ScheduleDriver::update_runtime_data() {
    return update_values(retrieveCurrentValues());
}

DeviceOperationResult ScheduleDriver::update_values(const std::array<std::optional<int>, NumChannels> &values) {
    auto lampDriverConf = reinterpret_cast<const ScheduleDriverData *>(mConf->device_config.data());

    for (unsigned int i = 0; i < values.size(); ++i) {
        const auto &currentDeviceIndex = lampDriverConf->deviceIndices[i];
        if (currentDeviceIndex) {
            const auto newValue = device_values::create_from_unit(lampDriverConf->channelUnit[i], *values[*currentDeviceIndex]);
            auto setResult = set_device_action(*currentDeviceIndex, newValue, nullptr, 0);
        }
    }

    updateScheduleState();

    return DeviceOperationResult::ok;

}

bool ScheduleDriver::updateScheduleState() {
    std::fill(scheduleState.channelTime.begin(), scheduleState.channelTime.end(), sinceWeekBeginning<std::chrono::seconds>());
    return remoteScheduleState.writeData(reinterpret_cast<char *>(&scheduleState), sizeof(ScheduleState));
}

std::array<std::optional<int>, NumChannels> ScheduleDriver::retrieveCurrentValues() {
    std::array<std::optional<int>, NumChannels> values;
    if (mConf == nullptr) {
        return values;
    }

    auto lampDriverConf = reinterpret_cast<const ScheduleDriverData *>(mConf->device_config.data());

    if (lampDriverConf->type == DriverType::Interpolate) {
        const auto currentDay = getDayOfWeek();
        const auto timeOfDaySeconds = getTimeOfDay<std::chrono::seconds>();

        // TODO: maybe add possibillity to search for the next timepoint which contains a specific channel
        const auto currentTimePoint = schedule.findCurrentTimePoint(timeOfDaySeconds);
        const auto nextTimePoint = schedule.findNextTimePoint(timeOfDaySeconds);

        // TODO: current timepoint is not the actual current timepoint time, use the time of the timepoint!
        for (uint8_t i = 0; i < std::tuple_size<decltype(decltype(schedule)::TimePointDataType::percentages)>::value; ++i) {
            const auto &currentChannelName = lampDriverConf->channelNames[i];
            const auto &currentDeviceIndex = lampDriverConf->deviceIndices[i];
            if (currentChannelName.has_value() && currentDeviceIndex.has_value()) {
                if (currentTimePoint && currentTimePoint->second.percentages[i]
                    && nextTimePoint && nextTimePoint->second.percentages[i]) {
                    const auto difference = static_cast<int>(*nextTimePoint->second.percentages[i]) - static_cast<int>(*currentTimePoint->second.percentages[i]);
                    std::chrono::seconds timeDifference = currentTimePoint->first - nextTimePoint->first;
                    if (std::abs(timeDifference.count()) < 1) {
                        timeDifference = decltype(timeDifference)(1);
                    }
                    const std::chrono::seconds currentTimePointInEffectSince = 
                        (std::chrono::hours{24 * static_cast<int>(currentDay)} + timeOfDaySeconds) - currentTimePoint->first;
                    const auto interpolationFactor = std::fabs(currentTimePointInEffectSince.count() / static_cast<float>(timeDifference.count()));

                    const auto newValue = *currentTimePoint->second.percentages[i] + difference * interpolationFactor;
                    Logger::log(LogLevel::Info, "%.*s(%d)=%f , interpFactor=%f",
                        (int) lampDriverConf->channelNames[i]->len(), lampDriverConf->channelNames[i]->data(), *currentDeviceIndex,
                        newValue, interpolationFactor);

                    values[i] = newValue;
                }
            }
        }
    } else if (lampDriverConf->type == DriverType::Action) {
        Logger::log(LogLevel::Info, "Checking possible actions");
        const auto timeOfDaySeconds = getTimeOfDay<std::chrono::seconds>();

        // TODO: maybe add possibillity to search for the next timepoint which contains a specific channel
        // TODO: implement currenttimepoint only on this day, otherwise the action of the last day would be repeated
        const auto currentTimePoint = schedule.findCurrentTimePoint(timeOfDaySeconds);

        // If the state time is "in the future", assume day wrap around and reset
        const auto timeInWeek = sinceWeekBeginning<std::chrono::seconds>();
        for (auto &currentChannelTime : scheduleState.channelTime) {
            if (currentChannelTime > timeInWeek) {
                using namespace std::chrono_literals;
                Logger::log(LogLevel::Info, "Wrapping channeltime around -> new week");
                currentChannelTime = 0s;
            }
        }

        for (uint8_t i = 0; i < std::tuple_size<decltype(decltype(schedule)::TimePointDataType::percentages)>::value; ++i) {
            const auto &currentChannelName = lampDriverConf->channelNames[i];
            const auto &currentDeviceIndex = lampDriverConf->deviceIndices[i];
            if (currentChannelName.has_value() && currentDeviceIndex.has_value()) {
                if (currentTimePoint && currentTimePoint->second.percentages[i]) {
                    // This action was already triggered
                    if (scheduleState.channelTime[i] > currentTimePoint->first) {
                        Logger::log(LogLevel::Info, "Event was already triggered");
                        continue;
                    } else {
                        Logger::log(LogLevel::Info, "Triggering event at %d > %d", 
                            (int) scheduleState.channelTime[i].count(), 
                            (int) currentTimePoint->first.count());
                    }

                    const auto newValue = *currentTimePoint->second.percentages[i];

                    Logger::log(LogLevel::Info, "%.*s(%d) action(%d)",
                        (int) lampDriverConf->channelNames[i]->len(), lampDriverConf->channelNames[i]->data(), *currentDeviceIndex,
                        newValue);

                    // TODO: maybe use same datatype which is already used in json stuff
                    values[i] = newValue;
                }
            }
        }
    }
    return values;
}