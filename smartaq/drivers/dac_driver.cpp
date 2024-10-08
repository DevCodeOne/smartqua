#include "build_config.h"

#ifdef ENABLE_DAC_DRIVER

#include "dac_driver.h"

#include <optional>
#include <type_traits>
#include <string_view>
#include <cstring>
#include <limits>
#include <algorithm>

#include "driver/dac.h"
#include "frozen.h"

#include "actions/device_actions.h"
#include "drivers/device_types.h"
#include "utils/utils.h"

DacDriver::DacDriver(const DeviceConfig *config, std::shared_ptr<dac_resource> dacResource) : m_conf(config), m_dac(dacResource) {
    if (dacResource && config != nullptr) {
        dac_output_enable(dacResource->channel_num());
        DeviceValues toSet = DeviceValues::create_from_unit(DeviceValueUnit::voltage, m_value);
        write_value("", toSet);
    }
}

std::optional<DacDriver> DacDriver::create_driver(const DeviceConfig *config) {
    auto *dacConf = reinterpret_cast<DacDriverData *>(config->device_config.data());
    auto dacResource = device_resource::get_dac_resource(dacConf->channel);

    if (dacResource == nullptr) {
        Logger::log(LogLevel::Warning, "Couldn't get dac resource");
        return std::nullopt;
    }

    if (dacConf == nullptr)  {
        Logger::log(LogLevel::Warning, "Dacconf isn't available");
        return std::nullopt;
    }

    Logger::log(LogLevel::Info, "Created dac resource");

    return std::make_optional(DacDriver{config, dacResource});
}

DeviceOperationResult DacDriver::write_value(std::string_view what, const DeviceValues &value) {
    if (m_conf == nullptr || m_dac == nullptr) {
        return DeviceOperationResult::failure;
    }

    auto voltageValue = value.voltage();

    if (!voltageValue.has_value() && value.percentage().has_value()) {
        static constexpr auto MaxPercentageValue = 100;
        const auto clampedPercentage = std::clamp<decltype(value.percentage())::value_type>(*value.percentage(), 0, MaxPercentageValue);
        voltageValue = static_cast<decltype(value.voltage())::value_type>((MaxVoltage / MaxPercentageValue) * clampedPercentage);
    }

    if (!voltageValue.has_value()) {
        return DeviceOperationResult::not_supported;
    }

    const auto targetValue = static_cast<uint8_t>(std::clamp(*voltageValue / MaxVoltage, 0.0f, 1.0f) * std::numeric_limits<uint8_t>::max());
    m_value = targetValue;

    Logger::log(LogLevel::Info, "Setting new value %d from voltage %f", targetValue, *voltageValue);

    esp_err_t result = dac_output_voltage(m_dac->channel_num(), targetValue);

    if (result != ESP_OK) {
        return DeviceOperationResult::failure;
    }


    return DeviceOperationResult::ok;
}

DeviceOperationResult DacDriver::read_value(std::string_view what, DeviceValues &value) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult DacDriver::get_info(char *output, size_t output_buffer_len) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult DacDriver::call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::not_supported;
}


DeviceOperationResult DacDriver::update_runtime_data() {
    return DeviceOperationResult::ok;
}

std::optional<DacDriver> DacDriver::create_driver(const std::string_view &input, DeviceConfig&device_conf_out) {
    DacDriverData newConf{};
    int channel = static_cast<int>(newConf.channel);

    json_scanf(input.data(), input.size(), "{ channel : %d }", &channel);

    bool assignResult = true;
    assignResult &= check_assign(newConf.channel, std::clamp<int>(channel - 1, 0, 1));

    if (!assignResult) {
        Logger::log(LogLevel::Warning, "Some value(s) were out of range");
        return std::nullopt;
    }
    
    std::memcpy(reinterpret_cast<DacDriverData *>(device_conf_out.device_config.data()), &newConf, sizeof(DacDriverData));
    return create_driver(&device_conf_out);
};

#endif