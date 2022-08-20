#pragma once

#include <cstdint>
#include <array>
#include <shared_mutex>
#include <string_view>
#include <optional>

#include "driver/ledc.h"

#include "drivers/device_types.h"
#include "drivers/device_resource.h"
#include "hal/gpio_types.h"

struct pwm_config {
    timer_config timer_conf {};
    uint16_t frequency = 100;
    ledc_channel_t channel = LEDC_CHANNEL_0;
    // TODO: configure this differently
    uint16_t max_value = (1 << 10) - 1;
    uint16_t current_value = 0;
    uint8_t gpio_num = static_cast<uint8_t>(gpio_num_t::GPIO_NUM_MAX);
    bool fade = false;
    bool invert = false;
};

class PwmDriver final {
    public:
        static inline constexpr char name[] = "pwm_driver";

        ~PwmDriver() = default;

        static std::optional<PwmDriver> create_driver(const std::string_view input, device_config &device_conf_out);
        static std::optional<PwmDriver> create_driver(const device_config *config);

        DeviceOperationResult write_value(const device_values &value);
        DeviceOperationResult read_value(device_values &value) const;
        DeviceOperationResult write_device_options(const char *json_input, size_t input_len);
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult update_runtime_data();
    private:
        PwmDriver(const device_config *conf, std::shared_ptr<timer_resource> timer, std::shared_ptr<gpio_resource> gpio, std::shared_ptr<led_channel> channel);

        const device_config *m_conf = nullptr;
        std::shared_ptr<timer_resource> m_timer = nullptr;
        std::shared_ptr<gpio_resource> m_gpio = nullptr;
        std::shared_ptr<led_channel> m_channel = nullptr;
};