#pragma once

#include <cstdint>
#include <optional>

#include "driver/ledc.h"

struct pwm_config {
    uint16_t frequency = 100;
    ledc_timer_t timer = LEDC_TIMER_0;
    ledc_channel_t channel = LEDC_CHANNEL_0;
    uint16_t max_value = (1 << 16) - 1;
    uint16_t current_value = 0;
    uint8_t gpio_num = std::numeric_limits<uint8_t>::max();
    bool fade = false;
};

// TODO: consider using pointer to pwm_config instead
class pwm final {
    public:
        pwm(const pwm_config *pwm_conf);
        ~pwm() = default;

        // TODO: return bool to indicate success or failure, or simply esp_err_t
        void update_value();
    private:
        const pwm_config *m_pwm_conf = nullptr;
};

std::optional<pwm> create_pwm_output(const pwm_config *pwm_conf);