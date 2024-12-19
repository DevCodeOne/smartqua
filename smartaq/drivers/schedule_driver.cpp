#include "schedule_driver.h"

#include <charconv>
#include <chrono>
#include <cstdio>

#include "build_config.h"
#include "ctre.hpp"

#include "drivers/device_types.h"
#include "actions/device_actions.h"
#include "frozen.h"
#include "utils/filesystem_utils.h"
#include "utils/json_utils.h"
#include "utils/schedule.h"
// #include "utils/sd_filesystem.h"
#include "utils/idf_utils.h"
#include "utils/logger.h"
#include "utils/time_utils.h"
#include "smartqua_config.h"

// TODO: maybe fix target value range 0-100
static constexpr inline ctll::fixed_string schedule_regexp{R"((?<hours>[0-1][\d]|[2][0-3])-(?<mins>[0-5][0-9]):(?<vars>(?:\w{0,10}=[\d.]*,?)*);?)"};
static constexpr inline ctll::fixed_string varextraction_regexp{R"((?:(?<variable>\w{0,10})=(?<value>[\d.]*),?))"};

static constexpr inline ctll::fixed_string hours_name{"hours"};
static constexpr inline ctll::fixed_string mins_name{"mins"};
static constexpr inline ctll::fixed_string vars_name{"vars"};
static constexpr inline ctll::fixed_string variable_name{"variable"};
static constexpr inline ctll::fixed_string value_name{"value"};

constexpr unsigned int numberOfBits(unsigned int x) {
    return x < 2 ? x : 1 + numberOfBits(x >> 1);
}

std::optional<ScheduleDriver::ScheduleType::DayScheduleType> parseDaySchedule(const ScheduleDriver &driver, const std::string_view &strValue) {
    ScheduleDriver::ScheduleType::DayScheduleType generatedSchedule;
    bool containsData = false;

    for (auto timePointMatch : ctre::search_all<schedule_regexp>(strValue)) {

        containsData = false;

        const auto hoursView = timePointMatch.get<hours_name>().to_view();
        const auto minsView = timePointMatch.get<mins_name>().to_view();

        uint32_t hoursValue = 0;
        uint32_t minsValue = 0;
        std::from_chars(hoursView.data(), hoursView.data() + hoursView.size(), hoursValue);
        std::from_chars(minsView.data(), minsView.data() + minsView.size(), minsValue);
        const std::chrono::seconds timeOfDay = std::chrono::hours(hoursValue) + std::chrono::minutes(minsValue);

        ValueTimePoint timePointData{};
        for (auto variableMatch : ctre::search_all<varextraction_regexp>(timePointMatch.get<vars_name>())) {
            const auto variableView = variableMatch.get<variable_name>().to_view();
            const auto valueView = variableMatch.get<value_name>().to_view();

            // TODO: check if this is safe
            const BasicStackString<16> terminatedCopy{valueView};
            char *end = 0;
            float value = std::strtof(terminatedCopy.data(), &end);

            // TODO: think what to do on error
            if (end == terminatedCopy.data()) {
                Logger::log(LogLevel::Warning, "Couldn't parse float value in ScheduleDriver data");
            }

            // TODO: if this fails, should the whole op fail ?
            if (auto channelIndex = driver.channelIndex(variableView)) {
                timePointData.values[*channelIndex] = value;
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

std::optional<ScheduleDriver> ScheduleDriver::create_driver(const std::string_view &input, DeviceConfig &device_conf_out) {
    // Only use temporary variable to initialize the real storage
    auto createdConf = device_conf_out.accessConfig<ScheduleDriverData>();
    {
        ScheduleDriverData newConf{};

        // TODO: Use device_names instead of the indices
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
        for (uint8_t i = 0; i < newConf.deviceIndices.size(); ++i) {
            if (newConf.channelNames[i].has_value() != newConf.deviceIndices[i].has_value()) {
                missingData = true;
                break;
            }
        }

        if (missingData) {
            return std::nullopt;
        }

        std::memcpy(device_conf_out.device_config.data(), &newConf, sizeof(ScheduleDriverData));
    }
    
    std::tm timeinfo;
    std::time_t now;

    wait_for_clock_sync(&now, &timeinfo);

    // Id is based on the devices used in the channels
    auto createId = [](const auto &indicesArray) {
        constexpr auto bitsForDevices = numberOfBits(max_num_devices);
        uint16_t id = 0;
        for (unsigned int i = 0; i < indicesArray.size(); ++i) {
            if (indicesArray[i]) {
                id |= static_cast<uint16_t>(*indicesArray[i]) << static_cast<uint16_t>(bitsForDevices * i);
            }
        }

        return id;
    };

    createdConf->creationId = createId(createdConf->deviceIndices);

    auto result = snprintf(createdConf->schedulePath.data(), decltype(createdConf->schedulePath)::ArrayCapacity, 
        schedulePathFormat, DefaultStorage::path.value, createdConf->creationId);

    if (result < 0) {
        Logger::log(LogLevel::Error, "The schedule path was too long");
        return std::nullopt;
    }

    result = snprintf(createdConf->scheduleStatePath.data(), decltype(createdConf->scheduleStatePath)::ArrayCapacity, 
        scheduleStatePathFormat, DefaultStorage::path.value, createdConf->creationId);

    if (result < 0) {
        Logger::log(LogLevel::Error, "The schedule state path was too long");
        return std::nullopt;
    }

    
    ScheduleDriver driver(&device_conf_out);
    if (!driver.loadAndUpdateSchedule(input)) {
        Logger::log(LogLevel::Warning, "Couldn't load and update schedule");
        return std::nullopt;
    }

    return driver;
}

std::optional<ScheduleDriver> ScheduleDriver::create_driver(const DeviceConfig *device_conf_out) {
    if (device_conf_out == nullptr) {
        return std::nullopt;
    }

    auto createdConf = device_conf_out->accessConfig<ScheduleDriverData>();
    auto buffer = LargeBufferPoolType::get_free_buffer();

    if (!buffer.has_value()) {
        Logger::log(LogLevel::Warning, "Couldn't get a free buffer");
        return std::nullopt;
    }

    ScheduleDriver driver(device_conf_out);
    auto result = loadFileCompletelyIntoBuffer(createdConf->schedulePath, buffer->data(), buffer->size());

    if (result <= 0) {
        Logger::log(LogLevel::Warning, "Couldn't open schedule file %s", createdConf->schedulePath.data());
        return std::nullopt;
    }

    std::string_view bufferView(buffer->data(), std::min(safe_strlen(buffer->data(), buffer->size()), static_cast<size_t>(result)));

    Logger::log(LogLevel::Info, "Parsing schedule ...");

    if (!driver.loadAndUpdateSchedule(bufferView)) {
        return std::nullopt;
    }

    return driver;
}

bool ScheduleDriver::loadAndUpdateSchedule(const std::string_view &input) {
    auto createdConf = mConf->accessConfig<ScheduleDriverData>();
    json_scanf(input.data(), input.size(), "{ schedule : %M }", json_scanf_single<ScheduleDriver>, this);
    Logger::log(LogLevel::Info, "Writing schedule to %s", createdConf->schedulePath.data());

    if (!safeWriteToFile(createdConf->schedulePath, ".tmp", input)) {
        Logger::log(LogLevel::Error, "Failed to write schedule, next reboot will have no schedule data !");
        return false;
    }

    ScheduleState newState;
    if (!loadFileCompletelyIntoBuffer(createdConf->scheduleStatePath, (void *) &newState, sizeof(ScheduleState))) {
        Logger::log(LogLevel::Warning, "Failed to read schedule state");
    }

    scheduleState = newState;
    return true;
}

std::optional<uint8_t> ScheduleDriver::channelIndex(std::string_view channelName) const {
    if (mConf == nullptr) {
        return std::nullopt;
    }

    auto scheduleDriverConf = mConf->accessConfig<ScheduleDriverData>();

    for (uint8_t i = 0; i < scheduleDriverConf->channelNames.size(); ++i) {
        if (!scheduleDriverConf->channelNames[i].has_value()) {
            continue;
        }

        if (scheduleDriverConf->channelNames[i]->getStringView() == channelName) {
            return i;
        }
    }
    return std::nullopt;
}

ScheduleDriver::ScheduleDriver(const DeviceConfig *conf) : mConf(conf) {}

DeviceOperationResult ScheduleDriver::write_value(std::string_view what, const DeviceValues &value) {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult ScheduleDriver::read_value(std::string_view what, DeviceValues &value) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult ScheduleDriver::get_info(char *output, size_t output_buffer_len) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult ScheduleDriver::call_device_action(DeviceConfig *conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult ScheduleDriver::update_runtime_data() {
    return update_values(retrieveCurrentValues());
}

DeviceOperationResult ScheduleDriver::update_values(const ChannelData &values) {
    auto scheduleDriverConf = mConf->accessConfig<ScheduleDriverData>();

    for (int i = 0; i < values.size(); ++i) {
        const auto &currentValue = values[i];
        if (currentValue) {
            const auto &[currentDeviceIndex, currentChannelTime, valueToSet] = *currentValue;

            Logger::log(LogLevel::Info, "Creating with channel_unit %d", (int) scheduleDriverConf->channelUnit[i]);
            const auto newValue = DeviceValues::create_from_unit(scheduleDriverConf->channelUnit[i], valueToSet);
            auto setResult = set_device_action(currentDeviceIndex, std::string_view(), newValue, nullptr, 0);
        }   else {
            Logger::log(LogLevel::Debug, "Nothing to set");
        }
    }

    updateScheduleState(values);

    return DeviceOperationResult::ok;

}

bool ScheduleDriver::updateScheduleState(const ChannelData &values) {
    auto scheduleDriverConf = mConf->accessConfig<ScheduleDriverData>();

    // All types except the single shot action type, don't need to store their state
    if (scheduleDriverConf->type != DriverType::Action) {
        return true;
    }

    bool anyValueSet = false;
    for (size_t i = 0; i < scheduleState.channelTime.size(); ++i) {
        if (values[i]) {
            scheduleState.channelTime[i] = std::get<1>(*values[i]);
            anyValueSet = true;
        }
    }

    if (anyValueSet) {
        Logger::log(LogLevel::Debug, "Updating remote schedule state");

        if (!safeWriteToFile(scheduleDriverConf->scheduleStatePath, ".tmp", (void *) &scheduleState, sizeof(scheduleState))) {
            Logger::log(LogLevel::Error, "Failed to write schedule state, next reboot will have no schedule data !");
            return false;
        }
    }

    return true;
}

auto ScheduleDriver::retrieveCurrentValues() -> ChannelData {
    ChannelData values;
    Logger::log(LogLevel::Info, "Schedule::retrieveCurrentValues");
    if (mConf == nullptr) {
        return values;
    }

    auto scheduleDriverConf = mConf->accessConfig<ScheduleDriverData>();

    if (scheduleDriverConf->type == DriverType::Interpolate) {
        Logger::log(LogLevel::Info, "Interpolating correct values");
        const auto currentDay = getDayOfWeek();
        const auto timeOfDaySeconds = getTimeOfDay<std::chrono::seconds>();
        const auto secondsSinceWeekBeginning = sinceWeekBeginning<std::chrono::seconds>();

        // TODO: maybe add possibility to search for the next timepoint which contains a specific channel
        const auto currentTimePoint = schedule.findCurrentTimePoint(timeOfDaySeconds);
        const auto nextTimePoint = schedule.findNextTimePoint(timeOfDaySeconds);

        // TODO: current timepoint is not the actual current timepoint time, use the time of the timepoint!
        for (uint8_t i = 0; i < std::tuple_size<decltype(decltype(schedule)::TimePointDataType::values)>::value; ++i) {
            const auto &currentChannelName = scheduleDriverConf->channelNames[i];
            const auto &currentDeviceIndex = scheduleDriverConf->deviceIndices[i];
            if (!currentChannelName.has_value() || !currentDeviceIndex.has_value()) {
                continue;
            }

            if (!currentTimePoint || !currentTimePoint->second.values[i]) {
                continue;
            }

            if (!nextTimePoint || !nextTimePoint->second.values[i]) {
                continue;
            }

            const auto difference = *nextTimePoint->second.values[i] - *currentTimePoint->second.values[i];
            std::chrono::seconds timeDifference = currentTimePoint->first - nextTimePoint->first;
            if (std::abs(timeDifference.count()) < 1) {
                timeDifference = decltype(timeDifference)(1);
            }
            const std::chrono::seconds currentTimePointInEffectSince =
                (std::chrono::hours{24 * static_cast<int>(currentDay)} + timeOfDaySeconds) - currentTimePoint->first;
            const auto interpolationFactor = std::fabs(currentTimePointInEffectSince.count() / static_cast<float>(timeDifference.count()));

            const auto newValue = *currentTimePoint->second.values[i] + difference * interpolationFactor;
            Logger::log(LogLevel::Info, "%.*s(%d)=%f , interpFactor=%f",
                (int) scheduleDriverConf->channelNames[i]->len(), scheduleDriverConf->channelNames[i]->data(), *currentDeviceIndex,
                newValue, interpolationFactor);

            values[i] = std::make_tuple(*currentDeviceIndex, secondsSinceWeekBeginning, newValue);
        }
    } else if (scheduleDriverConf->type == DriverType::Action || scheduleDriverConf->type == DriverType::ActionHold) {
        Logger::log(LogLevel::Info, "Checking possible actions");
        const auto timeOfDaySeconds = getTimeOfDay<std::chrono::seconds>();
        const auto secondsSinceWeekBeginning = sinceWeekBeginning<std::chrono::seconds>();

        // TODO: maybe add possibility to search for the next timepoint which contains a specific channel
        const auto currentTimePoint = schedule.findCurrentTimePoint(timeOfDaySeconds, DaySearchSettings::AllDays);
        const auto currentTimePointToday = schedule.findCurrentTimePoint(timeOfDaySeconds, DaySearchSettings::OnlyThisDay);

        // If the state time is "in the future", assume day wrap around and reset
        const auto timeInWeek = sinceWeekBeginning<std::chrono::seconds>();

        for (uint8_t i = 0; i < std::tuple_size<decltype(decltype(schedule)::TimePointDataType::values)>::value; ++i) {
            const auto &currentChannelName = scheduleDriverConf->channelNames[i];
            const auto &currentDeviceIndex = scheduleDriverConf->deviceIndices[i];
            if (!currentChannelName.has_value() || !currentDeviceIndex.has_value()) {
                continue;
            }

            if (!currentTimePoint || !currentTimePoint->second.values[i]) {
                continue;
            }

            // This action was already triggered, continue if the schedule type is single shot action
            if (scheduleState.channelTime[i] < currentTimePoint->first /* event should be triggered, channel time indicates not triggered event */
            || (scheduleState.channelTime[i] >= timeInWeek /* wrap around -> new week, channel time later then current time point */
                && currentTimePointToday
                && timeOfDaySeconds > currentTimePointToday->first)
            || scheduleDriverConf->type != DriverType::Action) {
                if (scheduleState.channelTime[i] > timeInWeek) {
                    Logger::log(LogLevel::Info, "Wrap around event");
                }

                if (currentTimePointToday) {
                    Logger::log(LogLevel::Info, "Triggering event because of a wrap around");
                } else {
                    Logger::log(LogLevel::Info, "Triggering event at %d > %d, %d, %d",
                        (int) scheduleState.channelTime[i].count(),
                        (int) currentTimePoint->first.count(),
                        (int) currentTimePointToday.has_value(),
                        (int) timeOfDaySeconds.count());
                }

            } else {
                Logger::log(LogLevel::Info, "Event was already triggered channelTime : %d, timeInWeek %d",
                    (int) scheduleState.channelTime[i].count(), (int) timeInWeek.count());

                continue;
            }

            const auto newValue = *currentTimePoint->second.values[i];

            Logger::log(LogLevel::Info, "%.*s(%d) action(%d)",
                (int) scheduleDriverConf->channelNames[i]->len(), scheduleDriverConf->channelNames[i]->data(), *currentDeviceIndex,
                newValue);

            // TODO: maybe use same datatype which is already used in json stuff
            values[i] = std::make_tuple(*currentDeviceIndex, currentTimePoint->first, newValue);
        }
    } else {
        Logger::log(LogLevel::Info, "Schedule type is not possible");
    }
    return values;
}