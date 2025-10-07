#pragma once

#include <cstdint>
#include <memory>
#include <thread>
#include <string_view>
#include <thread>
#include <expected>

#include "build_config.h"
#include "actions/action_types.h"
#include "actions/device_actions.h"
#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "utils/stack_string.h"
#include "utils/check_assign.h"

#include "driver/rmt_types.h"


// TODO: Max-/Min- On-/Off- Time Maybe?
enum struct SwitchType {
    LevelHolder,
    Invalid
};

enum struct SwitchDefaultValue {
    Low, High, Neutral
};

enum struct SwitchValueGenerationType
{
    Simple, Interpolated
};

enum struct BoundCalculationError
{
    ReadError, CalculationError
};

template<>
struct read_from_json<SwitchType> {
    static void read(const char *str, int len, SwitchType &type) {
        std::string_view input(str, len);

        type = SwitchType::Invalid;

        if (input == "LevelHolder") {
            type = SwitchType::LevelHolder;
        }
    }
};

template<>
struct read_from_json<SwitchDefaultValue> {
    static void read(const char *str, int len, SwitchDefaultValue &value) {
        std::string_view input(str, len);

        value = SwitchDefaultValue::Neutral;

        if (input == "Low" || input == "low") {
            value = SwitchDefaultValue::Low;
        } else if (input == "High" || input == "high") {
            value = SwitchDefaultValue::High;
        } else if (input == "Neutral" || input == "neutral")
        {
            value = SwitchDefaultValue::Neutral;
        }
    }
};

// TODO: optional add backup probe and control device
// TODO: also add some kind of alarm, when both devices report different values, which are too different
struct SwitchConfig {
    SwitchType type;
    SwitchDefaultValue defaultValue;
    unsigned int targetDeviceId;
    unsigned int readingDeviceId;
    unsigned int stayInState;
    bool onlyTriggerOnChange;
    DeviceValueUnit deviceUnit;
    DeviceValues targetValue;
    DeviceValues lowValueArgument;
    DeviceValues neutralValueArgument;
    DeviceValues highValueArgument;
    DeviceValues maxAllowedDifference;
    BasicStackString<MaxArgumentLength> readingArgument;
    BasicStackString<MaxArgumentLength> targetArgument;
};

template<typename C = std::chrono::steady_clock>
class SwitchDriver final {
    public:
        using ClockType = C;
        static constexpr char name[] = "SwitchDriver";

        SwitchDriver(const SwitchDriver &other) = delete;
        SwitchDriver(SwitchDriver &&other) noexcept;
        ~SwitchDriver() = default;

        SwitchDriver &operator=(const SwitchDriver &other) = delete;
        SwitchDriver &operator=(SwitchDriver &&other) noexcept = default;

        static std::expected<SwitchDriver, const char *> create_driver(const std::string_view& input, DeviceConfig &deviceConfOut);
        static std::expected<SwitchDriver, const char *> create_driver(const DeviceConfig *config);
        static std::expected<DeviceValues, ::BoundCalculationError> calculateRelevantBound(SwitchConfig* switchConfig,
            const DeviceValues& result);

        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const { return DeviceOperationResult::not_supported; }
        DeviceOperationResult call_device_action(DeviceConfig *conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data(); 

    private:
        explicit SwitchDriver(const DeviceConfig *conf);

        void enableSafety();
        bool writeValueToDevice(const DeviceValues& value);
        DeviceOperationResult watchValuesAndReact();

        using TimePoint = ClockType::time_point;

        TimePoint mSwitchedStatesAt;
        const DeviceConfig *mConf;
        std::expected<DeviceValues, BoundCalculationError> mPreviousValue;
};

template<typename C>
std::expected<SwitchDriver<C>, const char *> SwitchDriver<C>::create_driver(const std::string_view& input, DeviceConfig &deviceConfOut) {
    SwitchConfig newConf{
        .type = SwitchType::LevelHolder,
        .defaultValue = SwitchDefaultValue::Low,
        .targetDeviceId = InvalidDeviceId,
        .readingDeviceId = InvalidDeviceId,
        .stayInState = 10,
        .onlyTriggerOnChange = false,
        .deviceUnit = DeviceValueUnit::generic_unsigned_integral,
        .targetValue = {},
        .lowValueArgument = DeviceValues::create_empty(),
        .neutralValueArgument = DeviceValues::create_empty(),
        .highValueArgument = DeviceValues::create_empty(),
        .maxAllowedDifference = DeviceValues::create_empty(),
        .readingArgument{""},
        .targetArgument{""}
    };

    json_token readingArgument {nullptr, 0};
    json_token targetArgument{nullptr, 0};
    int targetDeviceId;
    int readingDeviceId;

    // TODO: add parsing for default value and switch type
    // TODO: both low and highValue, as well as targetValue have to be set, check that
    // TODO: prevent target_id or reading_id, to be the id of the switch
    json_scanf(input.data(), input.size(),
               "{ target_id : %d, reading_id : %d, reading_argument : %T, target_argument : %T, stay_in_state_seconds : %u, only_trigger_on_change : %B,"
               "default_value %M, "
               "target_value : %M, low_value_arg : %M, neutral_value_arg : %M, high_value_arg : %M, allowed_difference : %M"
               "device_unit : %M }",
               &targetDeviceId,
               &readingDeviceId,
               &readingArgument,
               &targetArgument,
               &newConf.stayInState,
               &newConf.onlyTriggerOnChange,
               json_scanf_single<SwitchDefaultValue>, &newConf.defaultValue,
               json_scanf_single<DeviceValues>, &newConf.targetValue,
               json_scanf_single<DeviceValues>, &newConf.lowValueArgument,
               json_scanf_single<DeviceValues>, &newConf.neutralValueArgument,
               json_scanf_single<DeviceValues>, &newConf.highValueArgument,
               json_scanf_single<DeviceValues>, &newConf.maxAllowedDifference,
               json_scanf_single<DeviceValueUnit>, &newConf.deviceUnit);

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
    }

    newConf.readingArgument.set(readingArgument.ptr, readingArgument.len);

    if (targetArgument.ptr != nullptr
        && targetArgument.len > newConf.targetArgument.size()) {
        Logger::log(LogLevel::Error, "Setting argument was too long");
        return std::unexpected{"Setting argument was too long"};
    }

    newConf.targetArgument.set(targetArgument.ptr, targetArgument.len);

    deviceConfOut.insertConfig(&newConf);

    return create_driver(&deviceConfOut);
}

template<typename C>
std::expected<SwitchDriver<C>, const char *> SwitchDriver<C>::create_driver(const DeviceConfig *config) {
    return { SwitchDriver{config} };
}

template <typename C>
[[nodiscard]] std::expected<DeviceValues, BoundCalculationError> SwitchDriver<C>::calculateRelevantBound(
    SwitchConfig* switchConfig,
    const DeviceValues& result)
{
    using DifferenceType = float;

    const auto difference = switchConfig->targetValue.difference(result);
    if (!difference.has_value())
    {
        Logger::log(LogLevel::Error, "Couldn't calculate difference between units");
        return std::unexpected(BoundCalculationError::CalculationError);
    }

    const auto maxAllowedDifference = switchConfig->maxAllowedDifference.getAsUnit<DifferenceType>();

    if (!maxAllowedDifference.has_value())
    {
        Logger::log(LogLevel::Error, "Couldn't represent max allowed difference value as float");
        return std::unexpected(BoundCalculationError::ReadError);
    }

    const auto differenceAsFloat = difference->getAsUnit<DifferenceType>();

    if (!differenceAsFloat.has_value() || !maxAllowedDifference.has_value())
    {
        Logger::log(LogLevel::Error, "Couldn't represent difference value as float");
        return std::unexpected(BoundCalculationError::ReadError);
    }

    if (std::fabs(*maxAllowedDifference) > std::fabs(*differenceAsFloat))
    {
        Logger::log(LogLevel::Debug, "Difference is small enough switch to do the neutral action [%f, %f] < %f",
                    -*maxAllowedDifference, *maxAllowedDifference, *differenceAsFloat);
        return { switchConfig->neutralValueArgument };
    }

    Logger::log(LogLevel::Debug, "Difference is large enough for this switch to do a low or high action %f < %f",
                *maxAllowedDifference, *differenceAsFloat);
    // targetValue > currentValue => lowValue
    if (differenceAsFloat < 0) {
        return { switchConfig->lowValueArgument };
    }

    return { switchConfig->highValueArgument };
    // If the values couldn't be compared, there should be some kind of error handling, best at the creation of the object
}

template<typename C>
DeviceOperationResult SwitchDriver<C>::watchValuesAndReact() {
    auto switchConfig = mConf->accessConfig<SwitchConfig>();
    // TODO: really check valueChanged

    Logger::log(LogLevel::Info, "SwitchDriver::watchValues entry");

    const auto readDeviceValueResult = readDeviceValue(switchConfig->readingDeviceId,
                                                       switchConfig->readingArgument.getStringView());

    if (!readDeviceValueResult)
    {
        Logger::log(LogLevel::Error, "Couldn't read from the device, setting default values ...");
        enableSafety();
        return DeviceOperationResult::ok;
    }

    std::expected<DeviceValues, BoundCalculationError> valueToSet
        = calculateRelevantBound(switchConfig, *readDeviceValueResult);

    if (!valueToSet)
    {
        Logger::log(LogLevel::Info, "Couldn't calculate new value, skipping ...");
        enableSafety();
        return DeviceOperationResult::ok;
    }

    if (mSwitchedStatesAt + std::chrono::seconds(switchConfig->stayInState) > ClockType::now())
    {
        Logger::log(LogLevel::Info, "Switch should still remain in state, still to early to switch, skipping ...");
        return DeviceOperationResult::ok;
    }

    const bool valueChanged = valueToSet != mPreviousValue;
    const bool shouldSetValue = valueChanged || switchConfig->onlyTriggerOnChange == false;

    if (!shouldSetValue)
    {
        Logger::log(LogLevel::Info, "Value didn't change so no new value is being send to the device");
        return DeviceOperationResult::ok;
    }

    if (writeValueToDevice(*valueToSet))
    {
        Logger::log(LogLevel::Info, "Set new value to device %d", switchConfig->targetDeviceId);
    }
    else
    {
        Logger::log(LogLevel::Error, "Couldn't execute action");
    }

    mPreviousValue = *valueToSet;
    return DeviceOperationResult::ok;
}

template<typename C>
SwitchDriver<C>::SwitchDriver(const DeviceConfig *config) : mConf(config) { }

template <typename C>
void SwitchDriver<C>::enableSafety()
{
    const auto config = mConf->accessConfig<SwitchConfig>();
    auto defaultValue = config->defaultValue == SwitchDefaultValue::Low
                                  ? config->lowValueArgument
                                  : (config->defaultValue == SwitchDefaultValue::Neutral
                                         ? config->neutralValueArgument
                                         : config->highValueArgument);
    writeValueToDevice(defaultValue);
}

template <typename C>
bool SwitchDriver<C>::writeValueToDevice(const DeviceValues &value)
{
    const auto config = mConf->accessConfig<SwitchConfig>();

    if (writeDeviceValue(config->targetDeviceId, config->targetArgument.getStringView(), value, true))
    {
        mSwitchedStatesAt = ClockType::now();
        return true;
    }

    return false;
}

template<typename C>
SwitchDriver<C>::SwitchDriver(SwitchDriver &&other) noexcept : mConf(other.mConf) { other.mConf = nullptr; }

template<typename C>
DeviceOperationResult SwitchDriver<C>::read_value(std::string_view what, DeviceValues &value) const {
    if (what == "targetValue")
    {
        value = mConf->accessConfig<SwitchConfig>()->targetValue;
        return DeviceOperationResult::ok;
    }

    return DeviceOperationResult::not_supported;
}

template<typename C>
DeviceOperationResult SwitchDriver<C>::write_value(std::string_view what, const DeviceValues& value)
{
    auto switchConfig = mConf->accessConfig<SwitchConfig>();

    if (value.getUnit() != switchConfig->deviceUnit)
    {
        Logger::log(LogLevel::Error, "Unit of value is not the same as the unit of the switch");
        return DeviceOperationResult::failure;
    }

    switchConfig->targetValue = value;
    Logger::log(LogLevel::Info, "Got new target value for switch");
    return DeviceOperationResult::ok;
}

template<typename C>
DeviceOperationResult SwitchDriver<C>::update_runtime_data() {
    return watchValuesAndReact();
}

template<typename C>
DeviceOperationResult SwitchDriver<C>::call_device_action(DeviceConfig *conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::not_supported;
}