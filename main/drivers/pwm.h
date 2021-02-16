#pragma once

#include <cstdint>
#include <array>
#include <shared_mutex>
#include <string_view>
#include <optional>

#include "driver/ledc.h"
#include "drivers/device_types.h"

/*template<size_t N>
struct print_to_json<pwm_setting_trivial<N>> {
    static int print(json_out *out, pwm_setting_trivial<N> &setting) {
        return json_printf(out, "%M", json_printf_array<decltype(setting.data)>, &setting.data);
    }
};

template<>
struct print_to_json<pwm_config> {
    static int print(json_out *out, pwm_config &config) {
        return json_printf(out, "{ %Q : %d, %Q : %d, %Q : %d, %Q : %d, %Q : %d, %Q : %d, %Q : %B }",
        "frequency", config.frequency, "timer", static_cast<int>(config.timer),
        "channel", static_cast<int>(config.channel),
        "max_value", static_cast<int>(config.max_value),
        "gpio_num", static_cast<int>(config.gpio_num),
        "current_value", static_cast<int>(config.current_value),
        "fade", config.fade);
    }
};
*/

struct pwm_config {
    uint16_t frequency = 100;
    ledc_timer_t timer = LEDC_TIMER_0;
    ledc_channel_t channel = LEDC_CHANNEL_0;
    uint16_t max_value = (1 << 16) - 1;
    uint16_t current_value = 0;
    uint8_t gpio_num = std::numeric_limits<uint8_t>::max();
    bool fade = false;
};

// TODO: multiple channels
class pwm final {
    public:
        static inline constexpr char name[] = "pwm_driver";

        ~pwm() = default;

        static std::optional<pwm> create_driver(const std::string_view input, device_config &device_conf_out);
        static std::optional<pwm> create_driver(const device_config *config);

        device_operation_result write_value(const device_values &value);
        device_operation_result read_value(device_values &value) const;
    private:
        pwm(const device_config *conf);

        const device_config *m_conf = nullptr;
        static inline std::array<std::optional<gpio_num_t>, max_num_devices> _pwm_gpios;
        static inline std::shared_mutex _instance_mutex;
};