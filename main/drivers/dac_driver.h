#pragma once

#include <array>
#include <optional>

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "smartqua_config.h"

struct DacDriverData final {
    dac_channel_t channel = dac_channel_t::DAC_CHANNEL_1;
    uint8_t value = 0;
};

class DacDriver final {
    public:
        static inline constexpr char name[] = "dac_driver";
        static inline constexpr float maxVoltage = 3.3;

        DacDriver(const DacDriver &other) = delete;
        DacDriver(DacDriver &&other) = default;
        ~DacDriver() = default;

        DacDriver &operator=(const DacDriver &other) = delete;
        DacDriver &operator=(DacDriver &&other) = default;

        static std::optional<DacDriver> create_driver(const std::string_view &input, device_config &device_conf_out);
        static std::optional<DacDriver> create_driver(const device_config *config);

        device_operation_result write_value(const device_values &value);
        device_operation_result read_value(device_values &value) const;
        device_operation_result get_info(char *output, size_t output_buffer_len) const;
        device_operation_result write_device_options(const char *json_input, size_t input_len);
        device_operation_result update_runtime_data();

    private:
        DacDriver(const device_config *conf, std::shared_ptr<dac_resource> dacResource);

        const device_config *m_conf;
        std::shared_ptr<dac_resource> m_dac;

};