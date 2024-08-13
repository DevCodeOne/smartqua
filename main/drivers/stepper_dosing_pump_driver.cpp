#include "stepper_dosing_pump_driver.h"

#include <chrono>
#include <cstddef>
#include <cstring>
#include <thread>

#include "actions/device_actions.h"
#include "build_config.h"
#include "driver/rmt_encoder.h"
#include "drivers/device_types.h"
#include "hal/gpio_types.h"
#include "drivers/device_resource.h"
#include "drivers/rmt_stepper_driver.h"
#include "utils/check_assign.h"

#include "driver/rmt_tx.h"
#include "driver/rmt_types.h"
#include "frozen.h"

std::optional<StepperDosingPumpDriver> StepperDosingPumpDriver::create_driver(const std::string_view input, DeviceConfig&device_conf_out) {
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

    std::memcpy(reinterpret_cast<StepperDosingConfig *>(device_conf_out.device_config.data()), &newConf, sizeof(StepperDosingConfig));

    return create_driver(&device_conf_out);
}

std::optional<StepperDosingPumpDriver> StepperDosingPumpDriver::create_driver(const DeviceConfig *config) {
    return StepperDosingPumpDriver{config};
}

StepperDosingPumpDriver::StepperDosingPumpDriver(const DeviceConfig *config)  : mConf(config) {}

// TODO: safe value of mStepsLeft in seperate remotevariable
DeviceOperationResult StepperDosingPumpDriver::write_value(std::string_view what, const DeviceValues &value) {
    const StepperDosingConfig *const stepperConfig = reinterpret_cast<const StepperDosingConfig *>(mConf->device_config.data());
    if (!value.milliliter()) {
        Logger::log(LogLevel::Error, "A dosing pump only supports values in ml");
        return DeviceOperationResult::failure;
    }

    Logger::log(LogLevel::Info, "Dosing %d ml", (int) *value.milliliter());
    auto stepsToDo = *value.milliliter() * (stepperConfig->stepsTimesTenPerMl / 10);
    const auto valueToSet = DeviceValues::create_from_unit(DeviceValueUnit::generic_unsigned_integral, stepsToDo);
    writeDeviceValue(stepperConfig->deviceId, stepperConfig->writeArgument, valueToSet, true);

    return DeviceOperationResult::ok;
}

DeviceOperationResult StepperDosingPumpDriver::call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json) {
    StepperDosingConfig *const stepperConfig = reinterpret_cast<StepperDosingConfig *>(conf->device_config.data());
    if (action == "manual") {
        int steps = 0;
        float milliliter = 0;
        json_scanf(json.data(), json.size(), "{ steps : %d, ml : %f}", &steps, &milliliter);

        if (milliliter != 0) {
            steps = static_cast<int>(milliliter * (stepperConfig->stepsTimesTenPerMl / 10.0f));
        }

        const auto valueToSet = DeviceValues::create_from_unit(DeviceValueUnit::generic_unsigned_integral, steps);
        writeDeviceValue(stepperConfig->deviceId, stepperConfig->writeArgument, valueToSet, true);
    } else if (action == "callibrate") {
        int steps = 200;
        float milliliter = 1.0f;
        json_scanf(json.data(), json.size(), "{ steps : %d, ml : %f }", &steps, &milliliter);

        Logger::log(LogLevel::Info, "%.*s, %d, %d", (int) json.size(), json.data(), steps, milliliter);
        stepperConfig->stepsTimesTenPerMl = static_cast<uint16_t>((steps * 10) / milliliter);
    }

    return DeviceOperationResult::ok;
}