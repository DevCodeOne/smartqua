#include "lamp_driver.h"

#include <charconv>

#include "ctre.hpp"

#include "drivers/device_types.h"
#include "actions/device_actions.h"
#include "frozen.h"
#include "utils/filesystem_utils.h"
#include "utils/sd_filesystem.h"
#include "utils/idf_utils.h"
#include "aq_main.h"

// TODO: maybe fix target value range 0-100
static constexpr inline ctll::fixed_string schedule_regexp{R"((?<hours>[0-1][\d]|[2][0-3])-(?<mins>[0-5][0-9]):(?<vars>(?:\w{0,10}=\d*,?)*);?)"};
static constexpr inline ctll::fixed_string varextraction_regexp{R"((?:(?<variable>\w{0,10})=(?<value>\d*),?))"};

static constexpr inline ctll::fixed_string hours_name{"hours"};
static constexpr inline ctll::fixed_string mins_name{"mins"};
static constexpr inline ctll::fixed_string vars_name{"vars"};
static constexpr inline ctll::fixed_string variable_name{"variable"};
static constexpr inline ctll::fixed_string value_name{"value"};

std::optional<LampDriver::ScheduleType::DayScheduleType> parseDaySchedule(const LampDriver &driver, const std::string_view &strValue) {
    LampDriver::ScheduleType::DayScheduleType generatedSchedule;
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

        LampTimePoint timePointData{};
        for (auto variableMatch : ctre::range<varextraction_regexp>(timePointMatch.get<vars_name>())) {
            const auto variableView = variableMatch.get<variable_name>().to_view();
            const auto valueView = variableMatch.get<value_name>().to_view();
            uint8_t valueAsInt = 0;

            std::from_chars(valueView.data(), valueView.data() + valueView.size(), valueAsInt);

            // TODO: if this fails, should the whole op fail ?
            if (auto channelIndex = driver.channelIndex(variableView)) {
                timePointData.percentages[*channelIndex] = valueAsInt;
            } else {
                ESP_LOGW("LampDriver", "Couldn't find channel %.*s", variableView.length(), variableView.data());
            }

            containsData = true;
        }

        if (containsData) {
            // ESP_LOGI("LampDriver", "Inserting Timepoint into schedule");
            generatedSchedule.insertTimePoint(std::make_pair(timeOfDay, timePointData));
        } else {
            const auto varsView = timePointMatch.get<vars_name>().to_view();
            ESP_LOGW("LampDriver", "%.*s didn't cointain any parseable data", 
                varsView.length(), varsView.data());
        }

    }

    if (!containsData) {
        return std::nullopt;
    }

    return generatedSchedule;
}

std::optional<LampDriver> LampDriver::create_driver(const std::string_view &input, device_config &device_conf_out) {
    // Only use temporary variable to initialize the real storage
    auto createdConf = reinterpret_cast<LampDriverData *>(device_conf_out.device_config.data());
    {
        LampDriverData newConf{};

        // TODO: Channel names should be equivalent to the actual device_names
        json_scanf(input.data(), input.size(), "{ channel_names : %M, devices : %M }", 
        json_scanf_array<decltype(newConf.channelNames)>, &newConf.channelNames,
        json_scanf_array<decltype(newConf.deviceIndices)>, &newConf.deviceIndices);

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

        std::memcpy(reinterpret_cast<LampDriverData *>(device_conf_out.device_config.data()), &newConf, sizeof(LampDriverData));
    }
    
    std::tm timeinfo;
    std::time_t now;

    wait_for_clock_sync(&now, &timeinfo);

    auto result = snprintf(createdConf->scheduleName.data(), decltype(createdConf->scheduleName)::ArrayCapacity, 
        path_format, sd_filesystem::mount_point, timeinfo.tm_year, timeinfo.tm_yday);

    if (result < 0) {
        ESP_LOGE("LampDriver", "Path to write the schedule to was too long");
        return std::nullopt;
    }
    
    LampDriver driver(&device_conf_out);
    if (!driver.loadAndUpdateSchedule(input)) {
        return std::nullopt;
    }

    return driver;
}

std::optional<LampDriver> LampDriver::create_driver(const device_config *device_conf_out) {
    if (device_conf_out == nullptr) {
        return std::nullopt;
    }

    auto createdConf = reinterpret_cast<LampDriverData *>(device_conf_out->device_config.data());
    auto buffer = LargeBufferPoolType::get_free_buffer();

    if (!buffer.has_value()) {
        return std::nullopt;
    }

    auto result = loadFileCompletelyIntoBuffer(createdConf->scheduleName, buffer->data(), buffer->size());

    if (result == -1) {
        ESP_LOGW("LampDriver", "Couldn't open schedule file %s", createdConf->scheduleName.data());
        return std::nullopt;
    }

    std::string_view bufferView(buffer->data(), safe_strlen(buffer->data(), buffer->size()));

    ESP_LOGI("LampDriver", "Parsing schedule %*s", (int) bufferView.length(), bufferView.data());
    LampDriver driver(device_conf_out);

    if (!driver.loadAndUpdateSchedule(bufferView)) {
        return std::nullopt;
    }

    return driver;
}

bool LampDriver::loadAndUpdateSchedule(const std::string_view &input) {
    auto createdConf = reinterpret_cast<const LampDriverData *>(mConf->device_config.data());
    json_scanf(input.data(), input.size(), "{ schedule : %M }", json_scanf_single<LampDriver>, this);
    ESP_LOGI("LampDriver", "Writing schedule to %s", createdConf->scheduleName.data());

    // TODO: maybe add Workaround for this
    if (!safeWriteToFile(createdConf->scheduleName, ".tmp", input)) {
        ESP_LOGE("LampDriver", "Failed to write schedule");
        return true;
    }
    return true;
}

std::optional<uint8_t> LampDriver::channelIndex(std::string_view channelName) const {
    if (mConf == nullptr) {
        return std::nullopt;
    }

    auto lampDriverConf = reinterpret_cast<const LampDriverData *>(mConf->device_config.data());

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

LampDriver::LampDriver(const device_config *conf) : mConf(conf) {}

DeviceOperationResult LampDriver::write_value(const device_values &value) {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult LampDriver::read_value(device_values &value) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult LampDriver::get_info(char *output, size_t output_buffer_len) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult LampDriver::write_device_options(const char *json_input, size_t input_len) {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult LampDriver::update_runtime_data() {
    if (mConf == nullptr) {
        return DeviceOperationResult::failure;
    }

    auto lampDriverConf = reinterpret_cast<const LampDriverData *>(mConf->device_config.data());
    const auto currentDay = getDayOfWeek();
    const auto timeOfDaySeconds = getTimeOfDay<std::chrono::seconds>();

    // TODO: maybe add possibillity to search for the next timepoint which contains a specific channel
    const auto currentTimePoint = schedule.findCurrentTimePoint(timeOfDaySeconds);
    const auto nextTimePoint = schedule.findNextTimePoint(timeOfDaySeconds);

    for (uint8_t i = 0; i < std::tuple_size<decltype(decltype(schedule)::TimePointDataType::percentages)>::value; ++i) {
        const auto &currentChannelName = lampDriverConf->channelNames[i];
        const auto &currentDeviceIndex = lampDriverConf->deviceIndices[i];
        if (currentChannelName && currentDeviceIndex) {
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
                ESP_LOGI("LampDriver", "%.*s(%d)=%f , interpFactor=%f",
                    (int) lampDriverConf->channelNames[i]->len(), lampDriverConf->channelNames[i]->data(), *currentDeviceIndex,
                    newValue, interpolationFactor);

                auto result = set_device_action(*currentDeviceIndex, device_values{.percentage = newValue}, nullptr, 0);
                // TODO: handle result
            }
        }
    }
    return DeviceOperationResult::ok;
}