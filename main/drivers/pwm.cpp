#include "pwm.h"

#include <mutex>
#include <cstring>

#include "frozen.h"
#include "esp_log.h"
#include "utils/utils.h"

pwm::pwm(const device_config *conf) : m_conf(conf) { }

device_operation_result pwm::write_value(const device_values &values) {
    auto *pwm_conf = reinterpret_cast<pwm_config *>(m_conf->device_config.data());
    esp_err_t result = ESP_FAIL;

    if (!values.generic_pwm.has_value()) {
        return device_operation_result::not_supported;
    }

    pwm_conf->current_value = *values.generic_pwm;
    ESP_LOGI("pwm_driver", "Setting new value %d", pwm_conf->current_value);

    if (!pwm_conf->fade) {
        result = ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, pwm_conf->channel, pwm_conf->current_value, 0);
    } else {
        // Maybe do the fading in a dedicated thread, and block that thread ?
        result = ledc_set_fade_time_and_start(LEDC_HIGH_SPEED_MODE, pwm_conf->channel, pwm_conf->current_value, 1000, ledc_fade_mode_t::LEDC_FADE_NO_WAIT);
    }

    return result == ESP_OK ? device_operation_result::ok : device_operation_result::failure;
}

device_operation_result pwm::read_value(device_values &values) const {
    return device_operation_result::not_supported;
}

std::optional<pwm> pwm::create_driver(const device_config *config) {
    static std::once_flag init_fading{};
    std::call_once(init_fading, [](){ 
        ESP_LOGI("pwm_driver", "Initialized fading");
        ledc_fade_func_install(0); 
    });

    auto *pwm_conf = reinterpret_cast<pwm_config *>(config->device_config.data());
    ledc_timer_config_t ledc_timer{};
    ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_timer.duty_resolution = LEDC_TIMER_13_BIT;
    ledc_timer.timer_num = pwm_conf->timer;
    ledc_timer.freq_hz = pwm_conf->frequency;
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;

    auto result = ledc_timer_config(&ledc_timer);

    if (result != ESP_OK) {
        ESP_LOGI("pwm_driver", "Couldn't create ledc driver");
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
        ESP_LOGI("pwm_driver", "Couldn't create ledc driver");
        return std::nullopt;
    }

    return std::make_optional(pwm{config});
}

// TODO: check if gpio is already in use
std::optional<pwm> pwm::create_driver(const std::string_view input, device_config &device_conf_out) {
    // Only prepare device_conf_out in this method and pass it along

    pwm_config new_conf{};
    int frequency = new_conf.frequency;
    int timer = new_conf.timer;
    int channel = new_conf.channel;
    int max_value = new_conf.max_value;
    int current_value = new_conf.current_value;
    int gpio_num = new_conf.gpio_num;
    bool fade = new_conf.fade;

    json_scanf(input.data(), input.size(),
        "{ frequency : %d, timer : %d, channel : %d, max_value : %d, current_value : %d, gpio_num : %d, fade : %B}", 
        &frequency, &timer, &channel, &max_value, &current_value, &gpio_num, &fade);

    bool assign_result = true;
    assign_result &= check_assign(new_conf.frequency, frequency);
    assign_result &= check_assign(new_conf.timer, timer);
    assign_result &= check_assign(new_conf.channel, channel);
    assign_result &= check_assign(new_conf.max_value, max_value);
    assign_result &= check_assign(new_conf.current_value, current_value);
    assign_result &= check_assign(new_conf.gpio_num, gpio_num);
    new_conf.fade = static_cast<bool>(fade);

    if (!assign_result) {
        // Some value was out of range
        return std::nullopt;
    }

    std::memcpy(reinterpret_cast<pwm_config *>(device_conf_out.device_config.data()), &new_conf, sizeof(pwm_config));
    return create_driver(&device_conf_out);
}