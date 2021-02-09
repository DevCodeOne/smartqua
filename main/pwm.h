#pragma once

#include <cstdint>

#include "driver/ledc.h"

struct pwm_config {
    uint16_t frequency = 100;
    ledc_timer_t timer = LEDC_TIMER_0;
    ledc_channel_t channel = LEDC_CHANNEL_0;
    uint16_t max_value = (1 << 16) - 1;
    uint8_t gpio_num;
    bool fade = false;
};

class pwm final {
    public:
        pwm(const pwm_config &pwm_conf);
        ~pwm() = default;

        void set_value();
    private:
        pwm_config m_pwm_conf;

        uint32_t m_value;
};