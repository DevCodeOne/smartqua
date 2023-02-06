#include "q30_driver.h"
#include "actions/action_types.h"
#include "actions/device_actions.h"
#include "drivers/device_types.h"


std::optional<Q30Driver> Q30Driver::create_driver(const std::string_view &input, device_config &device_conf_out) {
    auto createdConf = reinterpret_cast<Q30Data *>(device_conf_out.device_config.data());
    std::optional<ScheduleDriver> createdLampDriver = std::nullopt;
    createdConf->fanDevice = -1;
    createdConf->onSwitchDevice = -1;
    json_token scheduleData{};
    json_scanf(input.data(), input.size(), "{ fan_device : %d, on_device : %d, lamp_driver : %T}",
        &createdConf->fanDevice,
        &createdConf->onSwitchDevice,
        &scheduleData);
    
    std::string_view scheduleDataAsView(scheduleData.ptr, scheduleData.len);

    if (createdConf->fanDevice == -1 || createdConf->onSwitchDevice == -1 || scheduleData.ptr == nullptr) {
        Logger::log(LogLevel::Warning, "Couldn't create Q30 Driver : Missing fan_device and/or on_device and/or schedule");
        return std::nullopt;
    }

    createdLampDriver = ScheduleDriver::create_driver(scheduleDataAsView, device_conf_out);

    if (!createdLampDriver.has_value()) {
        Logger::log(LogLevel::Warning, "Couldn't create Q30 Driver : Couldn't create LampDriver");
        return std::nullopt;
    }

    //Logger::log(LogLevel::Warning, "Created Q30 Driver");
    return Q30Driver(std::move(createdLampDriver), &device_conf_out);
}

std::optional<Q30Driver> Q30Driver::create_driver(const device_config *config) {
    if (config == nullptr) {
        return std::nullopt;
    }

    auto createdLampDriver = ScheduleDriver::create_driver(config);

    if (!createdLampDriver.has_value()) {
        Logger::log(LogLevel::Warning, "Couldn't create LampDriver from stored config");
        return std::nullopt;
    }

    return Q30Driver(std::move(createdLampDriver), config);
}

Q30Driver::Q30Driver(std::optional<ScheduleDriver> &&lampDriver, const device_config *config) : mConf(config), mLampDriver(std::move(lampDriver)) { }

DeviceOperationResult Q30Driver::write_value(const device_values &value) {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult Q30Driver::read_value(device_values &value) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult Q30Driver::get_info(char *output, size_t output_buffer_len) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult Q30Driver::call_device_action(device_config *conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::not_supported;
}


DeviceOperationResult Q30Driver::update_runtime_data() {
    auto q30Conf = reinterpret_cast<const Q30Data *>(mConf->device_config.data());

    if (!mLampDriver.has_value()) {
        return DeviceOperationResult::failure;
    }

    auto newChannelValues = mLampDriver->retrieveCurrentValues();
    uint16_t fanSpeed = 0;

    // TODO: change algo later later
    for (const auto &currentValue : newChannelValues) {
        if (currentValue) {
            fanSpeed += *currentValue * 2;
        }
    }

    fanSpeed = std::min<uint16_t>(fanSpeed, 100u);
    Logger::log(LogLevel::Info, "Setting fanSpeed to %d percent", fanSpeed);

    const auto fanSetResult = set_device_action(q30Conf->fanDevice, device_values{.percentage = fanSpeed}, nullptr, 0);

    if (fanSetResult.result != json_action_result_value::successfull) {
        return DeviceOperationResult::failure;
    }

    return mLampDriver->update_values(newChannelValues);

}

