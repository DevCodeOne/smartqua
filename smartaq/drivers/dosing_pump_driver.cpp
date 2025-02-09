#include "dosing_pump_driver.h"

#include <chrono>
#include <cstddef>
#include <cstring>
#include <thread>

#include "actions/device_actions.h"
#include "build_config.h"
#include "driver/rmt_encoder.h"
#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "utils/check_assign.h"

#include "frozen.h"

std::optional<DosingPumpDriver> DosingPumpDriver::create_driver(const std::string_view input, DeviceConfig&device_conf_out) {
    StepperDosingConfig newConf {
        .deviceId = InvalidDeviceId
    };

    int deviceId = newConf.deviceId;
    json_token writeArgument;

    json_scanf(input.data(), input.size(), "{ deviceId : %d,  argument : %T }", &deviceId, &writeArgument);
      
    bool assignResult = true;

    assignResult &= checkAssign(newConf.deviceId, deviceId);

    if (!assignResult) {
        Logger::log(LogLevel::Error, "Some value(s) were out of range");
    }

    if (newConf.deviceId == InvalidDeviceId) {
        Logger::log(LogLevel::Error, "Invalid device id");
        return std::nullopt;
    }

    if (writeArgument.len > newConf.writeArgument.size()) {
        Logger::log(LogLevel::Error, "Write argument is too big");
        return std::nullopt;
    }

    newConf.writeArgument = std::string_view(writeArgument.ptr, writeArgument.len);

    std::memcpy(device_conf_out.device_config.data(), &newConf, sizeof(StepperDosingConfig));

    return create_driver(&device_conf_out);
}

std::optional<DosingPumpDriver> DosingPumpDriver::create_driver(const DeviceConfig *config) {
    return DosingPumpDriver{config};
}

DosingPumpDriver::DosingPumpDriver(const DeviceConfig *config)  : mConf(config) {}

constexpr uint16_t convertMilliliterToUnsignedIntegralValue(float milliliter, int unitTimesTenPerMl) {
    return static_cast<uint16_t>(milliliter * (static_cast<float>(unitTimesTenPerMl) / 10.0f));
}

// TODO: safe value of mStepsLeft in separate remotevariable
DeviceOperationResult DosingPumpDriver::write_value(std::string_view what, const DeviceValues &value) {
    auto dosingConfig = mConf->accessConfig<StepperDosingConfig>();
    if (!value.milliliter()) {
        Logger::log(LogLevel::Error, "A dosing pump only supports values in ml");
        return DeviceOperationResult::failure;
    }

    Logger::log(LogLevel::Info, "Dosing %d ml", (int) *value.milliliter());
    const auto unsignedIntegralValue = convertMilliliterToUnsignedIntegralValue(*value.milliliter(), dosingConfig->unitTimesTenPerMl);
    const auto valueToSet = DeviceValues::create_from_unit(DeviceValueUnit::generic_unsigned_integral, unsignedIntegralValue);
    writeDeviceValue(dosingConfig->deviceId, static_cast<std::string_view>(dosingConfig->writeArgument), valueToSet, true);

    return DeviceOperationResult::ok;
}

DeviceOperationResult DosingPumpDriver::call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json) {
    auto dosingConfig = conf->accessConfig<StepperDosingConfig>();

    unsigned int unsignedIntegralValue = 0;
    float milliliter = 0;
    json_scanf(json.data(), json.size(), "{ general_unsigned_integral : %u, ml : %f}", &unsignedIntegralValue, &milliliter);

    if (action == "manual") {
        if (milliliter != 0) {
            unsignedIntegralValue = static_cast<int>(milliliter * (static_cast<float>(dosingConfig->unitTimesTenPerMl) / 10.0f));
        }

        const auto valueToSet = DeviceValues::create_from_unit(DeviceValueUnit::generic_unsigned_integral, unsignedIntegralValue);
        writeDeviceValue(dosingConfig->deviceId, dosingConfig->writeArgument.getStringView(), valueToSet, true);
    } else if (action == "calibrate") {
        Logger::log(LogLevel::Info, "Calibrating dosing pump %.*s with unsigned value: %d -> %d / 10.0 ml", (int) json.size(), json.data(), unsignedIntegralValue, milliliter);
        dosingConfig->unitTimesTenPerMl = static_cast<uint16_t>(static_cast<float>(unsignedIntegralValue * 10) / milliliter);
    }

    return DeviceOperationResult::ok;
}