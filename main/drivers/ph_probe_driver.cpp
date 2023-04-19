#include "ph_probe_driver.h"

#include <limits>
#include <cmath>

#include "actions/device_actions.h"
#include "drivers/device_types.h"
#include "utils/utils.h"


std::optional<PhProbeDriver> PhProbeDriver::create_driver(const std::string_view input, device_config &device_conf_out) {
    static constexpr unsigned int INVALID_DEVICE_ID = 0xFF;
    PhProbeConfig newConf{
        .analogDeviceId = INVALID_DEVICE_ID,
        .temperatureDeviceId = INVALID_DEVICE_ID,
        .analogReadingArgument = "",
        .temperatureReadingArgument = ""
    };

    json_token analogReadingArgument {};
    json_token temperatureReadingArgument{};
    int analogDeviceId;
    int temperatureDeviceId;

    json_scanf(input.data(), input.size(), "{ analog_id : %d, temp_id : %d, analog_argument : %T, temp_argument : %T }",
        &analogDeviceId,
        &temperatureDeviceId,
        &analogReadingArgument,
        &temperatureReadingArgument);

    bool check = check_assign(newConf.analogDeviceId, analogDeviceId);
    check &= check_assign(newConf.temperatureDeviceId, temperatureDeviceId);

    if (!check) {
        Logger::log(LogLevel::Error, "Value(s) for analog device id and or temperature device id were out of range ");
        return std::nullopt;
    }

    if (newConf.analogDeviceId == INVALID_DEVICE_ID) {
        Logger::log(LogLevel::Error, "Invalid analog device id");
        return std::nullopt;
    }

    if (newConf.temperatureDeviceId == INVALID_DEVICE_ID) {
        Logger::log(LogLevel::Error, "Invalid temperature device id");
        return std::nullopt;
    }

    if (analogReadingArgument.ptr != nullptr 
        && analogReadingArgument.len > newConf.analogReadingArgument.size()) {
        Logger::log(LogLevel::Error, "Analog reading argument was too long");
        return std::nullopt;
    } else {
        newConf.analogReadingArgument = std::string_view(analogReadingArgument.ptr, 
            analogReadingArgument.len);
    }

    if (temperatureReadingArgument.ptr != nullptr 
        && temperatureReadingArgument.len > newConf.temperatureReadingArgument.size()) {
        Logger::log(LogLevel::Error, "Temperature argument was too long");
        return std::nullopt;
    } else {
        newConf.temperatureReadingArgument = std::string_view(temperatureReadingArgument.ptr, 
            temperatureReadingArgument.len); 
    }

    std::memcpy(reinterpret_cast<PhProbeConfig *>(device_conf_out.device_config.data()), 
        &newConf, sizeof(PhProbeConfig));

    return create_driver(&device_conf_out);
}

std::optional<PhProbeDriver> PhProbeDriver::create_driver(const device_config *config) {
    return std::optional{PhProbeDriver{config}};
}

PhProbeDriver::PhProbeDriver(const device_config *config) : mConf(config) { }

PhProbeDriver::PhProbeDriver(PhProbeDriver &&other) : mConf(std::move(other.mConf)) { }

PhProbeDriver &PhProbeDriver::operator=(PhProbeDriver &&other) {
    using std::swap;

    swap(mConf, other.mConf);

    return *this;
}

DeviceOperationResult PhProbeDriver::read_value(std::string_view what, device_values &value) const {
    const PhProbeConfig *const phConfig = reinterpret_cast<const PhProbeConfig *>(mConf->device_config.data());
    // TODO: read actual ph and temperature correction
    const auto result = readDeviceValue(phConfig->analogDeviceId, phConfig->analogReadingArgument.getStringView());

    if (!result || !result->generic_analog) {
        return DeviceOperationResult::failure;
    }

    const auto dx = phConfig->higherPhPair.analogReading - phConfig->lowerPhPair.analogReading;
    const auto dy = phConfig->higherPhPair.ph - phConfig->lowerPhPair.ph;
    
    if (dy == 0) {
        Logger::log(LogLevel::Info, "dx was zero, lowerPhPair a : %u ph : %f, higherPhPair a : %u, ph : %f",
            phConfig->lowerPhPair.analogReading, phConfig->lowerPhPair.ph,
            phConfig->higherPhPair.analogReading, phConfig->higherPhPair.ph);
        return DeviceOperationResult::failure;
    }

    Logger::log(LogLevel::Info, "current_reading : %u, lowerPhPair a : %u ph : %f, higherPhPair a : %u, ph : %f",
        *result->generic_analog,
        phConfig->lowerPhPair.analogReading, phConfig->lowerPhPair.ph,
        phConfig->higherPhPair.analogReading, phConfig->higherPhPair.ph);

    const float slope = dy / static_cast<float>(dx);

    Logger::log(LogLevel::Info, "slope: %f + offset: %f", slope, phConfig->lowerPhPair.ph);
    value.ph = phConfig->lowerPhPair.ph + (*result->generic_analog - phConfig->lowerPhPair.analogReading) * slope;

    return DeviceOperationResult::ok;
}

DeviceOperationResult PhProbeDriver::call_device_action(device_config *conf, const std::string_view &action, const std::string_view &json) {
    PhProbeConfig *phConfig = reinterpret_cast<PhProbeConfig *>(conf->device_config.data());
    // TODO: temperature correction
    if (action == "callibrate-lower" || action == "callibrate-higher") {
        unsigned int ph10x = std::numeric_limits<unsigned int>::max();
        json_scanf(json.data(), json.size(), "{ ph_10x : %u}", &ph10x);

        const auto result = readDeviceValue(phConfig->analogDeviceId, phConfig->analogReadingArgument.getStringView());
        if (!result || !result->generic_analog) {
            Logger::log(LogLevel::Error, "Device returned wrong type of measuremt or nothing at all");
            return DeviceOperationResult::failure;
        }

        const PhValuePair callibrationPoint{
            .analogReading = *result->generic_analog,
            .ph = ph10x / 10.0f
        };

        if (action == "callibrate-lower") {
            phConfig->lowerPhPair = callibrationPoint;
            return DeviceOperationResult::ok;
        } else {
            phConfig->higherPhPair = callibrationPoint;
            return DeviceOperationResult::ok;
        }
    }

    return DeviceOperationResult::failure;
}