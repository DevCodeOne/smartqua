#include "switch_driver.h"

#include <compare>
#include <limits>
#include <cmath>
#include <thread>
#include <chrono>
#include <string_view>
#include <expected>

#include "actions/action_types.h"
#include "actions/device_actions.h"
#include "drivers/device_types.h"
#include "utils/check_assign.h"


std::expected<SwitchDriver, const char *> SwitchDriver::create_driver(const std::string_view input, DeviceConfig &device_conf_out) {
    SwitchConfig newConf{
        .type = SwitchType::LevelHolder,
        .defaultValue = SwitchDefaultValue::Low,
        .targetDeviceId = InvalidDeviceId,
        .readingDeviceId = InvalidDeviceId,
        .targetValue = {},
        .lowValue = {},
        .highValue = {},
        .maxAllowedDifference = {},
        .readingArgument = "",
        .targetArgument = ""
    };

    json_token readingArgument {};
    json_token targetArgument{};
    int targetDeviceId;
    int readingDeviceId;

    // TODO: add parsing for default value and switch type
    // TODO: both low and highValue, as well as targetValue have to be set, check that
    // TODO: prevent target_id or reading_id, to be the id of the switch 
    json_scanf(input.data(), input.size(), "{ target_id : %d, reading_id : %d, reading_argument : %T, target_argument : %T, \
    target_value : %M, low_value : %M, high_value : %M, allowed_difference : %M }",
               &targetDeviceId,
               &readingDeviceId,
               &readingArgument,
               &targetArgument,
               json_scanf_single<DeviceValues>, &newConf.targetValue,
               json_scanf_single<DeviceValues>, &newConf.lowValue,
               json_scanf_single<DeviceValues>, &newConf.highValue,
               json_scanf_single<DeviceValues>, &newConf.maxAllowedDifference);

    bool check = checkAssign(newConf.readingDeviceId, readingDeviceId);
    check &= checkAssign(newConf.targetDeviceId, targetDeviceId);

    if (!check) {
        Logger::log(LogLevel::Error, "Value(s) for device id(s)");
        return std::unexpected{"Value(s) for device id(s)"};
    }

    if (newConf.readingDeviceId == InvalidDeviceId) {
        Logger::log(LogLevel::Error, "Invalid device id to read from");
        return std::unexpected{"Invalid device id to read from"};
    }

    if (newConf.targetDeviceId == InvalidDeviceId) {
        Logger::log(LogLevel::Error, "Invalid device id to write to");
        return std::unexpected{"Invalid device id to write to"};
    }

    if (readingArgument.ptr != nullptr 
        && readingArgument.len > newConf.readingArgument.size()) {
        Logger::log(LogLevel::Error, "Reading argument was too long");
        return std::unexpected{"Reading argument was too long"};
    } else {
        newConf.readingArgument = std::string_view(readingArgument.ptr, 
            readingArgument.len);
    }

    if (targetArgument.ptr != nullptr 
        && targetArgument.len > newConf.targetArgument.size()) {
        Logger::log(LogLevel::Error, "Setting argument was too long");
        return std::unexpected{"Setting argument was too long"};
    } else {
        newConf.targetArgument = std::string_view(targetArgument.ptr, 
            targetArgument.len); 
    }

    std::memcpy(device_conf_out.device_config.data(), &newConf, sizeof(SwitchConfig));

    return create_driver(&device_conf_out);
}

// This doesn't work for some reason
std::expected<SwitchDriver, const char *> SwitchDriver::create_driver(const DeviceConfig *config) {
    return { SwitchDriver{config} };
}


void SwitchDriver::watchValues(std::stop_token token, SwitchDriver *instance) {
    auto switchConfig = instance->mConf->accessConfig<SwitchConfig>();
    // TODO: really check valueChanged
    bool valueChanged = true;

    Logger::log(LogLevel::Info, "SwitchDriver::watchValues entry");
    while (!token.stop_requested()) {
        using namespace std::chrono_literals;

        const auto startTime = std::chrono::steady_clock::now();

        const auto result = readDeviceValue(switchConfig->readingDeviceId, switchConfig->readingArgument.getStringView());
        DeviceValues valueToSet;

        if (result.has_value()) {
            using DifferenceType = float;
            const auto difference = switchConfig->targetValue.difference<DifferenceType>(*result);
            const auto allowedDifference = switchConfig->maxAllowedDifference.getAsUnit<DifferenceType>();
            
            if (difference.has_value() && std::fabs(*allowedDifference) < std::fabs(*difference)) {
                Logger::log(LogLevel::Info, "Difference is large enough for this switch to do something %f < %f", 
                    *allowedDifference, *difference);
                valueChanged = true;
                // targetValue > currentValue => lowValue
                if (difference < 0) {
                    valueToSet = switchConfig->lowValue;
                } else {
                    valueToSet = switchConfig->highValue;
                }
            } else {
                Logger::log(LogLevel::Info, "Difference is too small for this switch to do anything %f < %f",
                    *allowedDifference, *difference);
            }
            // If the values couldn't be compared, there should be some kind of error handling, best at the creation of the object
        } else {
            Logger::log(LogLevel::Error, "Couldn't read from the device, setting default values ...");
            // Error reading state => default values
            if (switchConfig->defaultValue == SwitchDefaultValue::Low) {
                valueToSet = switchConfig->lowValue;
            } else {
                valueToSet = switchConfig->highValue;
            }
        }


        if (valueChanged) {
            auto view = switchConfig->targetArgument.getStringView();
            const auto result = writeDeviceValue(switchConfig->targetDeviceId, switchConfig->targetArgument, valueToSet, true);

            if (result) {
                Logger::log(LogLevel::Info, "Set new value to device %d", switchConfig->targetDeviceId);
            } else {
                Logger::log(LogLevel::Error, "Couldn't execute action");
            }

            valueChanged = false;
        }

        const auto timeDiff = std::chrono::steady_clock::now() - startTime;

        // TODO: make this tunable
         std::this_thread::sleep_for(timeDiff < 5s ? 5s - timeDiff : 500ms);
    }
    Logger::log(LogLevel::Info, "SwitchDriver::watchValues exit");

}

SwitchDriver::SwitchDriver(const DeviceConfig *config) : mConf(config) { }

SwitchDriver::SwitchDriver(SwitchDriver &&other) noexcept : mConf(other.mConf) { other.mConf = nullptr; }

SwitchDriver &SwitchDriver::operator=(SwitchDriver &&other) noexcept {
    using std::swap;

    other.mWatchValueThread.request_stop();
    if(other.mWatchValueThread.joinable()) {
        other.mWatchValueThread.join();
    }

    swap(mConf, other.mConf);

    return *this;
}

SwitchDriver::~SwitchDriver() {
    if (!mConf) {
        return;
    }

    mWatchValueThread.request_stop();
    if (mWatchValueThread.joinable()) {
        mWatchValueThread.join();
    }
}

DeviceOperationResult SwitchDriver::read_value(std::string_view what, DeviceValues &value) const {
    return DeviceOperationResult::ok;
}

// TODO: Fix thread start, maybe specific method for after initialization things
DeviceOperationResult SwitchDriver::update_runtime_data() { 
    Logger::log(LogLevel::Info, "SwitchDriver::update_runtime_data ");
    if (!mWatchValueThread.joinable()) {
        Logger::log(LogLevel::Info, "SwitchDriver::update_runtime_data start thread");
        mWatchValueThread = std::jthread(&SwitchDriver::watchValues, this);
    }
    return DeviceOperationResult::ok; 
}

DeviceOperationResult SwitchDriver::call_device_action(DeviceConfig *conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::not_supported;
}