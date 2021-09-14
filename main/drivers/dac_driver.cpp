#include "dac_driver.h"

#include <optional>
#include <type_traits>
#include <string_view>
#include <cstring>
#include <limits>
#include <algorithm>

#include "esp_log.h"
#include "driver/dac.h"
#include "frozen.h"

#include "actions/device_actions.h"
#include "drivers/device_types.h"
#include "utils/utils.h"

DacDriver::DacDriver(const device_config *config, std::shared_ptr<dac_resource> dacResource) : m_conf(config), m_dac(dacResource) {
    if (dacResource && config != nullptr) {
        dac_output_enable(dacResource->channel_num());
        const auto *dacConf = reinterpret_cast<DacDriverData *>(config->device_config.data());
        write_value(device_values{ .voltage = dacConf->value });
    }
}

std::optional<DacDriver> DacDriver::create_driver(const device_config *config) {
    auto *dacConf = reinterpret_cast<DacDriverData *>(config->device_config.data());
    auto dacResource = device_resource::get_dac_resource(dacConf->channel);

    if (dacResource == nullptr) {
        ESP_LOGW("DacDriver", "Couldn't get dac resource");
        return std::nullopt;
    }

    if (dacConf == nullptr)  {
        ESP_LOGW("DacDriver", "Dacconf isn't available");
        return std::nullopt;
    }

    ESP_LOGI("DacDriver", "Created dac resource");

    return std::make_optional(DacDriver{config, dacResource});
}

DeviceOperationResult DacDriver::write_value(const device_values &value) {
    if (m_conf == nullptr || m_dac == nullptr) {
        return DeviceOperationResult::failure;
    }

    auto *dacConf = reinterpret_cast<DacDriverData *>(m_conf->device_config.data());
    decltype(value.voltage) voltageValue = value.voltage;

    if (!voltageValue.has_value() && value.percentage.has_value()) {
        static constexpr auto MaxPercentageValue = 100;
        const auto clampedPercentage = std::clamp<decltype(value.percentage)::value_type>(*value.percentage, 0, MaxPercentageValue);
        voltageValue = static_cast<decltype(value.voltage)::value_type>((maxVoltage / MaxPercentageValue) * (clampedPercentage));
    }

    if (!voltageValue.has_value()) {
        return DeviceOperationResult::not_supported;
    }

    const auto targetValue = static_cast<uint8_t>(std::clamp(*voltageValue / maxVoltage, 0.0f, 1.0f) * std::numeric_limits<uint8_t>::max());

    ESP_LOGI("DacDriver", "Setting new value %d", targetValue);

    esp_err_t result = dac_output_voltage(m_dac->channel_num(), targetValue);

    if (result != ESP_OK) {
        return DeviceOperationResult::failure;
    }

    dacConf->value = targetValue;

    return DeviceOperationResult::ok;
}

DeviceOperationResult DacDriver::read_value(device_values &value) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult DacDriver::get_info(char *output, size_t output_buffer_len) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult DacDriver::write_device_options(const char *json_input, size_t input_len) {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult DacDriver::update_runtime_data() {
    return DeviceOperationResult::ok;
}

std::optional<DacDriver> DacDriver::create_driver(const std::string_view &input, device_config &device_conf_out) {
    DacDriverData newConf{};
    int channel = static_cast<int>(newConf.channel);
    int value = static_cast<int>(newConf.value);

    json_scanf(input.data(), input.size(), "{ channel : %d, value : %d }", &channel, &value);

    bool assignResult = true;
    assignResult &= check_assign(newConf.channel, std::clamp<int>(channel - 1, 0, 1));
    assignResult &= check_assign(newConf.value, value);

    if (!assignResult) {
        ESP_LOGW("DacDriver", "Some value(s) were out of range");
        return std::nullopt;
    }
    
    std::memcpy(reinterpret_cast<DacDriverData *>(device_conf_out.device_config.data()), &newConf, sizeof(DacDriverData));
    return create_driver(&device_conf_out);
};