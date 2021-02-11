#include <mutex>

#include "pwm.h"

#include "esp_log.h"

pwm::pwm(const pwm_config *pwm_conf) : m_pwm_conf(pwm_conf) { }

void pwm::update_value() {
    if (!m_pwm_conf->fade) {
        ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, m_pwm_conf->channel, m_pwm_conf->current_value, 0);
    } else {
        ledc_set_fade_time_and_start(LEDC_HIGH_SPEED_MODE, m_pwm_conf->channel, m_pwm_conf->current_value, 1000, ledc_fade_mode_t::LEDC_FADE_NO_WAIT);
    }
}

std::optional<pwm> create_pwm_output(const pwm_config *pwm_conf) {
    // TODO: somehow configure timers differently
    static std::once_flag init_fading{};
    std::call_once(init_fading, [](){ 
        ESP_LOGI("Init fading", "Initialized fading");
        ledc_fade_func_install(0); 
    });
    ledc_timer_config_t ledc_timer{};
    ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_timer.duty_resolution = LEDC_TIMER_13_BIT;
    ledc_timer.timer_num = pwm_conf->timer;
    ledc_timer.freq_hz = pwm_conf->frequency;
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;

    auto result = ledc_timer_config(&ledc_timer);

    if (result != ESP_OK) {
        return std::nullopt;
    }

    ledc_channel_config_t ledc_channel{};
    ledc_channel.gpio_num = pwm_conf->gpio_num;
    ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_channel.channel = LEDC_CHANNEL_0;
    ledc_channel.timer_sel = pwm_conf->timer;
    ledc_channel.duty = pwm_conf->current_value;
    ledc_channel.hpoint = 0;
    ledc_channel.intr_type = ledc_intr_type_t::LEDC_INTR_DISABLE;

    result = ledc_channel_config(&ledc_channel);

    if (result != ESP_OK) {
        return std::nullopt;
    }

    return std::make_optional(pwm{pwm_conf});
}