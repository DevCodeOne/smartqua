#pragma once

#include "build_config.h"

#ifdef ENABLE_DAC_DRIVER

#include <array>
#include <optional>

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "build_config.h"

struct DacDriverData final {
    dac_channel_t channel = dac_channel_t::DAC_CHANNEL_1;
};

class DacDriver final {
    public:
        static inline constexpr char name[] = "dac_driver";
        static inline constexpr float MaxVoltage = 3.3;

        DacDriver(const DacDriver &other) = delete;
        DacDriver(DacDriver &&other) = default;
        ~DacDriver() = default;

        DacDriver &operator=(const DacDriver &other) = delete;
        DacDriver &operator=(DacDriver &&other) = default;

        static std::optional<DacDriver> create_driver(const std::string_view &input, DeviceConfig&device_conf_out);
        static std::optional<DacDriver> create_driver(const DeviceConfig*config);

        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult update_runtime_data();

    private:
        DacDriver(const DeviceConfig*conf, std::shared_ptr<dac_resource> dacResource);

        const DeviceConfig*m_conf = nullptr;
        uint8_t m_value = 0;
        std::shared_ptr<dac_resource> m_dac = nullptr;

};

#endif