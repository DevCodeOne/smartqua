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

enum struct PinType {
    Input, Output, Pwm, Invalid
};

template<>
struct read_from_json<PinType> {
    static void read(const char *str, int len, PinType &type) {
        std::string_view input(str, len);

        type = PinType::Invalid;

        if (input == "Input" || input == "input" || input == "in") {
            type = PinType::Input;
        } else if (input == "Output" || input == "output" || input == "out") {
            type = PinType::Output;
        } else if (input == "PWM" || input == "Pwm" || input == "pwm") {
            type = PinType::Pwm;
        }
    }
};

struct PinConfig {
    PinType type;
    timer_config timer_conf {};
    uint16_t frequency = 100;
    ledc_channel_t channel = LEDC_CHANNEL_0;
    // TODO: configure this differently
    uint16_t max_value = (1 << 10) - 1;
    uint8_t gpio_num = static_cast<uint8_t>(gpio_num_t::GPIO_NUM_MAX);
    bool fade = false;
    bool invert = false;
};

class PinDriver final {
    public:
        static inline constexpr char name[] = "pin_driver";

        ~PinDriver() = default;

        static std::optional<PinDriver> create_driver(const std::string_view input, DeviceConfig&device_conf_out);
        static std::optional<PinDriver> create_driver(const DeviceConfig*config);

        DeviceOperationResult write_value(std::string_view what, const DeviceValues &value);
        DeviceOperationResult read_value(std::string_view what, DeviceValues &value) const;
        DeviceOperationResult call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json);
        DeviceOperationResult get_info(char *output, size_t output_buffer_len) const;
        DeviceOperationResult update_runtime_data();
    private:
        PinDriver(const DeviceConfig*conf, std::shared_ptr<timer_resource> timer, std::shared_ptr<gpio_resource> gpio, std::shared_ptr<led_channel> channel);

        const DeviceConfig*m_conf = nullptr;
        std::shared_ptr<timer_resource> m_timer = nullptr;
        std::shared_ptr<gpio_resource> m_gpio = nullptr;
        std::shared_ptr<led_channel> m_channel = nullptr;
        uint16_t m_current_value = 0;
};