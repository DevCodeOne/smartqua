#include "lamp_driver.h"

std::optional<LampDriver> LampDriver::create_driver(const std::string_view &input, device_config &device_conf_out) {
    LampDriverData newConf{};

    json_scanf(input.data(), input.size(), "{ channel_names : %M }", json_scanf_array<decltype(newConf.channelNames)>, &newConf.channelNames);

    for (const auto &currentName : newConf.channelNames) {
        if (currentName) {
            ESP_LOGI("LampDriver", "%s", currentName->data());
        }
    }

    return std::nullopt;
}

std::optional<LampDriver> LampDriver::create_driver(const device_config *device_conf_out) {
    return std::nullopt;
}

device_operation_result LampDriver::write_value(const device_values &value) {
    return device_operation_result::not_supported;
}

device_operation_result LampDriver::read_value(device_values &value) const {
    return device_operation_result::not_supported;
}

device_operation_result LampDriver::get_info(char *output, size_t output_buffer_len) const {
    return device_operation_result::not_supported;
}

device_operation_result LampDriver::write_device_options(const char *json_input, size_t input_len) {
    return device_operation_result::not_supported;
}