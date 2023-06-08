#include "switch_driver.h"

#include <compare>
#include <limits>
#include <cmath>
#include <thread>
#include <chrono>

#include "actions/action_types.h"
#include "actions/device_actions.h"
#include "drivers/device_types.h"
#include "utils/utils.h"


std::optional<SwitchDriver> SwitchDriver::create_driver(const std::string_view input, DeviceConfig &device_conf_out) {
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
    json_scanf(input.data(), input.size(), "{ target_id : %d, reading_id : %d, reading_argument : %T, target_argument : %T, \
    target_value : %M, low_value : %M, high_value : %M, allowed_difference : %M }",
        &targetDeviceId,
        &readingDeviceId,
        &readingArgument,
        &targetArgument,
        json_scanf_single<device_values>, &newConf.targetValue,
        json_scanf_single<device_values>, &newConf.lowValue,
        json_scanf_single<device_values>, &newConf.highValue,
        json_scanf_single<device_values>, &newConf.maxAllowedDifference);

    bool check = check_assign(newConf.readingDeviceId, readingDeviceId);
    check &= check_assign(newConf.targetDeviceId, targetDeviceId);

    if (!check) {
        Logger::log(LogLevel::Error, "Value(s) for device id(s)");
        return std::nullopt;
    }

    if (newConf.readingDeviceId == InvalidDeviceId) {
        Logger::log(LogLevel::Error, "Invalid device id to read from");
        return std::nullopt;
    }

    if (newConf.targetDeviceId == InvalidDeviceId) {
        Logger::log(LogLevel::Error, "Invalid device id to write to");
        return std::nullopt;
    }

    if (readingArgument.ptr != nullptr 
        && readingArgument.len > newConf.readingArgument.size()) {
        Logger::log(LogLevel::Error, "Reading argument was too long");
        return std::nullopt;
    } else {
        newConf.readingArgument = std::string_view(readingArgument.ptr, 
            readingArgument.len);
    }

    if (targetArgument.ptr != nullptr 
        && targetArgument.len > newConf.targetArgument.size()) {
        Logger::log(LogLevel::Error, "Setting argument was too long");
        return std::nullopt;
    } else {
        newConf.targetArgument = std::string_view(targetArgument.ptr, 
            targetArgument.len); 
    }

    std::memcpy(reinterpret_cast<SwitchConfig *>(device_conf_out.device_config.data()), 
        &newConf, sizeof(SwitchConfig));

    return create_driver(&device_conf_out);
}

std::optional<SwitchDriver> SwitchDriver::create_driver(const DeviceConfig *config) {
    return std::optional{SwitchDriver{config}};
}


void SwitchDriver::watchValues(std::stop_token token, SwitchDriver *instance) {
    const SwitchConfig *const switchConfig = reinterpret_cast<const SwitchConfig *>(instance->mConf->device_config.data());
    bool valueChanged = true;

    while (!token.stop_requested()) {
        using namespace std::chrono_literals;

        const auto startTime = std::chrono::steady_clock::now();

        const auto result = readDeviceValue(switchConfig->readingDeviceId, switchConfig->readingArgument.getStringView());
        device_values valueToSet;

        if (result.has_value()) {
            using DifferenceType = float;
            const auto difference = switchConfig->targetValue.difference<DifferenceType>(*result);
            
            if (switchConfig->maxAllowedDifference.getAsUnit<DifferenceType>() < difference) {
                valueChanged = true;
                // targetValue > currentValue => lowValue
                if (difference > 0) {
                    valueToSet = switchConfig->lowValue;
                } else {
                    valueToSet = switchConfig->highValue;
                }
            }
            // If the values couldn't be compared, there should be some kind of error handling, best at the creation of the object
        } else {
            // Error reading state => default values
            if (switchConfig->defaultValue == SwitchDefaultValue::Low) {
                valueToSet = switchConfig->lowValue;
            } else {
                valueToSet = switchConfig->highValue;
            }
        }


        if (valueChanged) {
            auto view = switchConfig->targetArgument.getStringView();
            const auto result =  writeDeviceValue(switchConfig->targetDeviceId, switchConfig->targetArgument, valueToSet);

            if (result) {
                Logger::log(LogLevel::Error, "Set new value to device");
            } else {
                Logger::log(LogLevel::Error, "Couldn't execute action");
            }

            valueChanged = false;
        }

        const auto timeDiff = std::chrono::steady_clock::now() - startTime;

        // TODO: make this tuneable
        std::this_thread::sleep_for(timeDiff < 1s ? 1s - timeDiff : 250ms);
    }

}

SwitchDriver::SwitchDriver(const DeviceConfig *config) : mConf(config) { 
    mWatchValueThread = std::jthread(&SwitchDriver::watchValues, this);
}

SwitchDriver::SwitchDriver(SwitchDriver &&other) : mConf(std::move(other.mConf)) {
    other.mWatchValueThread.request_stop();
    if(other.mWatchValueThread.joinable()) {
        other.mWatchValueThread.join();
    }

    other.mConf = nullptr;
    mWatchValueThread = std::jthread(&SwitchDriver::watchValues, this);
 }

SwitchDriver &SwitchDriver::operator=(SwitchDriver &&other) {
    using std::swap;

    other.mWatchValueThread.request_stop();
    if(other.mWatchValueThread.joinable()) {
        other.mWatchValueThread.join();
    }

    swap(mConf, other.mConf);

    mWatchValueThread = std::jthread(&SwitchDriver::watchValues, this);

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

DeviceOperationResult SwitchDriver::read_value(std::string_view what, device_values &value) const {
    const SwitchConfig *const switchConfig = reinterpret_cast<const SwitchConfig *>(mConf->device_config.data());
    return DeviceOperationResult::ok;
}

DeviceOperationResult SwitchDriver::call_device_action(DeviceConfig *conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::not_supported;
}