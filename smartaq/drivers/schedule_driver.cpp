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

#include "utils/esp/idf_utils.h"
#include "utils/time/schedule.h"
#include "utils/logger.h"
#include "smartqua_config.h"

// TODO: maybe fix target value range 0-100
static constexpr inline ctll::fixed_string schedule_regexp{R"((?<hours>[0-1][\d]|[2][0-3])-(?<mins>[0-5][0-9]):(?<vars>(?:\w{0,10}=[\d.]*,?)*);?)"};
static constexpr inline ctll::fixed_string varextraction_regexp{R"((?:(?<variable>\w{0,10})=(?<value>[\d.]*),?))"};

static constexpr inline ctll::fixed_string hours_name{"hours"};
static constexpr inline ctll::fixed_string mins_name{"mins"};
static constexpr inline ctll::fixed_string vars_name{"vars"};
static constexpr inline ctll::fixed_string variable_name{"variable"};
static constexpr inline ctll::fixed_string value_name{"value"};

static constexpr unsigned int numberOfBits(unsigned int x) {
    return x < 2 ? x : 1 + numberOfBits(x >> 1);
}

static void logTimePointData(const ScheduleDriver::ScheduleType::DayScheduleType &schedule) {
    for (const auto &timePoint : schedule) {
        Logger::log(LogLevel::Debug, "Timepoint @ %u :", static_cast<uint32_t>(timePoint.first.count()));

        for (uint8_t currentChannel = 0; currentChannel < timePoint.second.size(); ++currentChannel) {
            if (timePoint.second[currentChannel].has_value()) {
                Logger::log(LogLevel::Debug, "\t Timepoint data[%d] : %f", currentChannel, *timePoint.second[currentChannel]);
            }
        }
    }
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

        std::array<std::optional<float>, schedule_max_num_channels> timePointData{};
        for (auto variableMatch : ctre::search_all<varextraction_regexp>(timePointMatch.get<vars_name>())) {
            const auto variableView = variableMatch.get<variable_name>().to_view();
            const auto valueView = variableMatch.get<value_name>().to_view();

            // TODO: check if this is safe
            const BasicStackString<16> terminatedCopy{ valueView };
            char *end = 0;
            float value = std::strtof(terminatedCopy.data(), &end);

            if (end == terminatedCopy.data()) {
                Logger::log(LogLevel::Warning, "Couldn't parse float value %.*s in ScheduleDriver data", terminatedCopy.length(), terminatedCopy.data());
                return std::nullopt;
            }

            if (auto channelIndex = driver.channelIndex(variableView); channelIndex.has_value()) {
                timePointData[*channelIndex] = value;
                Logger::log(LogLevel::Debug, "Adding timepoint @ %u:%u with value : %f to channel : %d", hoursValue, minsValue, value, *channelIndex);
            } else {
                Logger::log(LogLevel::Warning, "Couldn't find channel %.*s", variableView.length(), variableView.data());
                return std::nullopt;
            }

            containsData = true;
        }

        if (containsData) {
            Logger::log(LogLevel::Debug, "Inserting time point into schedule");
            const auto couldInsert = generatedSchedule.insertTimePoint(timeOfDay, timePointData);

            if (!couldInsert) {
                Logger::log(LogLevel::Warning, "Couldn't insert time point into schedule");
                return std::nullopt;
            }
        } else {
            const auto varsView = timePointMatch.get<vars_name>().to_view();
            Logger::log(LogLevel::Warning, "%.*s didn't contain any parseable data",
                varsView.length(), varsView.data());
        }

    }

    if (!containsData) {
        return std::nullopt;
    }

    logTimePointData(generatedSchedule);

    return generatedSchedule;
}

ScheduleDriver::ScheduleDriver(ScheduleDriver &&other) noexcept : mConf(other.mConf), schedule(std::move(other.schedule)),
                                                                  scheduleTracker(&schedule) {
}

ScheduleDriver & ScheduleDriver::operator=(ScheduleDriver &&other) {
    mConf = other.mConf;
    schedule = other.schedule;
    scheduleTracker.setSchedule(&schedule);
    return *this;
}

std::optional<ScheduleDriver> ScheduleDriver::create_driver(const std::string_view &input, DeviceConfig &device_conf_out) {
    // Only use temporary variable to initialize the real storage
    auto createdConf = device_conf_out.accessConfig<ScheduleDriverData>();
    {
        ScheduleDriverData newConf{};

        // TODO: Use device_names instead of the indices
        // TODO: maybe do channel_desc, instead of the individual arrays
        json_scanf(input.data(), input.size(), "{ type : %M, channel_names : %M, devices : %M, channel_units : %M }", 
        json_scanf_single<ScheduleEventTransitionMode>, &newConf.type,
        json_scanf_array<decltype(newConf.channelNames)>, &newConf.channelNames,
        json_scanf_array<decltype(newConf.deviceIndices)>, &newConf.deviceIndices,
        json_scanf_array<decltype(newConf.channelUnit)>, &newConf.channelUnit);

        for (const auto &channelName : newConf.channelNames) {
            Logger::log(LogLevel::Debug, "Length of Channel name: %d %.*s", channelName->len(), channelName->len(), channelName->data());
        }

        bool missingData = false;
        for (uint8_t i = 0; i < newConf.deviceIndices.size(); ++i) {
            if (newConf.channelNames[i].has_value() != newConf.deviceIndices[i].has_value()) {
                Logger::log(LogLevel::Warning, "Channel %d has no device index nor a channel name", i);
                missingData = true;
                break;
            }
        }

        if (missingData) {
            Logger::log(LogLevel::Warning, "Couldn't create schedule driver, missing channel names or device indices");
            return std::nullopt;
        }

        std::memcpy(device_conf_out.device_config.data(), &newConf, sizeof(ScheduleDriverData));
    }
    
    std::tm timeinfo{};
    std::time_t now{};

    Logger::log(LogLevel::Debug, "Waiting for clock sync");
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
    auto result = loadFileCompletelyIntoBuffer(createdConf->schedulePath.getStringView(), buffer->data(), buffer->size());

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

    if (!safeWriteToFile(createdConf->schedulePath.getStringView(), ".tmp", input)) {
        Logger::log(LogLevel::Error, "Failed to write schedule, next reboot will have no schedule data !");
        return false;
    }

    scheduleTracker.setTrackingType(createdConf->type);

    return readChannelTimes();
}

bool ScheduleDriver::readChannelTimes() {
    auto createdConf = mConf->accessConfig<ScheduleDriverData>();

    Logger::log(LogLevel::Info, "Reading channel times of schedule");
    std::array<std::chrono::seconds, ScheduleType::Channels> channelTimes{};
    if (!loadFileCompletelyIntoBuffer(createdConf->scheduleStatePath.getStringView(), channelTimes.data(), sizeof(decltype(channelTimes)))) {
        Logger::log(LogLevel::Warning, "Failed to read schedule state");
        return false;
    }

    scheduleTracker.setChannelTimes(channelTimes);
    return true;
}

bool ScheduleDriver::updateChannelTimes() const {
    auto scheduleDriverConf = mConf->accessConfig<ScheduleDriverData>();

    const auto &channelTimes = scheduleTracker.getChannelTimes();

    if (!safeWritePodToFile(scheduleDriverConf->scheduleStatePath.getStringView(), ".tmp", &channelTimes)) {
        Logger::log(LogLevel::Error, "Failed to write schedule state, next reboot will have no schedule data !");
        return false;
    }

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

ScheduleDriver::ScheduleDriver(const DeviceConfig *conf) : mConf(conf), schedule(), scheduleTracker(&schedule) {}

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
    return updateValues();
}

DeviceOperationResult ScheduleDriver::updateValues() {
    auto scheduleDriverConf = mConf->accessConfig<ScheduleDriverData>();

    std::time_t now = std::time(nullptr);
    std::tm *currentDate = std::localtime(&now);
    bool wasUpdated = false;

    if (!currentDate) {
        Logger::log(LogLevel::Warning, "Couldn't get current date");
        return DeviceOperationResult::failure;
    }

    Logger::log(LogLevel::Debug, "ScheduleDriver::updateValues");

    const auto newValues = scheduleTracker.getCurrentChannelValues(*currentDate);

    for (int i = 0; i < newValues.size(); ++i) {
        if (!newValues[i].has_value()) {
            Logger::log(LogLevel::Debug, "Nothing to set");
            continue;
        }

        const auto currentValue = *newValues[i];

        if (scheduleDriverConf->deviceIndices.size() <= i || !scheduleDriverConf->deviceIndices[i].has_value()) {
            continue;
        }

        const auto currentDeviceIndex = *scheduleDriverConf->deviceIndices[i];

        Logger::log(LogLevel::Info, "Creating with channel_unit %d", (int) scheduleDriverConf->channelUnit[i]);
        const auto newValue = DeviceValues::create_from_unit(scheduleDriverConf->channelUnit[i], currentValue);
        auto setResult = set_device_action(currentDeviceIndex, std::string_view(), newValue, nullptr, 0);

        // TODO: check setResult

        scheduleTracker.updateChannelTime(i, *currentDate);
        wasUpdated = true;
    }

    if (wasUpdated) {
        updateChannelTimes();
    }

    return DeviceOperationResult::ok;

}