#include "pwm.h"

#include <algorithm>
#include <cstring>

#include "driver/ledc.h"
#include "drivers/device_resource.h"
#include "drivers/device_types.h"
#include "frozen.h"
#include "hal/ledc_types.h"
#include "utils/logger.h"
#include "utils/utils.h"

PwmDriver::PwmDriver(const device_config *conf, std::shared_ptr<timer_resource> timer, std::shared_ptr<gpio_resource> gpio, std::shared_ptr<led_channel> channel) 
: m_conf(conf), m_timer(timer), m_gpio(gpio), m_channel(channel) { }

DeviceOperationResult PwmDriver::write_value(const device_values &value) {
    auto *pwm_conf = reinterpret_cast<pwm_config *>(m_conf->device_config.data());
    esp_err_t result = ESP_FAIL;
    
    if (pwm_conf == nullptr) {
        return DeviceOperationResult::not_supported;
    }

    decltype(value.generic_pwm) pwmValue = 0;

    if (value.generic_pwm.has_value()) {
        pwmValue = *value.generic_pwm;
    } else if (value.percentage.has_value()) {
        static constexpr auto MaxPercentageValue = 100;
        const auto clampedPercentage = std::clamp<decltype(value.percentage)::value_type>(*value.percentage, 0, MaxPercentageValue);
        pwmValue = static_cast<decltype(pwmValue)::value_type>((pwm_conf->max_value / MaxPercentageValue) * clampedPercentage);
    }

    if (!pwmValue.has_value()) {
        return DeviceOperationResult::not_supported;
    }

    if (!pwm_conf->invert) {
        m_current_value = std::clamp<uint16_t>(*pwmValue, 0, pwm_conf->max_value);
    } else {
        m_current_value = pwm_conf->max_value - std::clamp<uint16_t>(*pwmValue, 0, pwm_conf->max_value);
    }

    result = ESP_OK;
    if (!pwm_conf->fade) {
        if (ledc_get_duty(pwm_conf->timer_conf.speed_mode, pwm_conf->channel) != m_current_value) {
            result = ledc_set_duty(pwm_conf->timer_conf.speed_mode, pwm_conf->channel, m_current_value);
            result = ledc_update_duty(pwm_conf->timer_conf.speed_mode, pwm_conf->channel);
            Logger::log(LogLevel::Info, "Setting new value %d", m_current_value);
        } else {
            Logger::log(LogLevel::Info, "New Value is the same as the old one %d", m_current_value);
        }
    } else {
        // Maybe do the fading in a dedicated thread, and block that thread ?
        if (ledc_get_duty(pwm_conf->timer_conf.speed_mode, pwm_conf->channel) != m_current_value) {
            result = ledc_set_fade_time_and_start(pwm_conf->timer_conf.speed_mode, pwm_conf->channel, m_current_value, 1000, ledc_fade_mode_t::LEDC_FADE_NO_WAIT);
            Logger::log(LogLevel::Info, "Setting new value %d", m_current_value);
        } else {
            Logger::log(LogLevel::Info, "New Value is the same as the old one %d", m_current_value);
        }
    }

    return result == ESP_OK ? DeviceOperationResult::ok : DeviceOperationResult::failure;
}

// TODO: implement both
DeviceOperationResult PwmDriver::read_value(std::string_view what, device_values &values) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult PwmDriver::call_device_action(device_config *conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::not_supported;
}


DeviceOperationResult PwmDriver::update_runtime_data() {
    return DeviceOperationResult::ok;
}

DeviceOperationResult PwmDriver::get_info(char *output_buffer, size_t output_buffer_len) const {
    json_out out = JSON_OUT_BUF(output_buffer, output_buffer_len);
    json_printf(&out, "{}");
    return DeviceOperationResult::ok;
}

std::optional<PwmDriver> PwmDriver::create_driver(const device_config *config) {
    static std::once_flag init_fading{};
    std::call_once(init_fading, [](){ 
        Logger::log(LogLevel::Info, "Initialized fading");
        ledc_fade_func_install(0); 
    });

    auto *pwm_conf = reinterpret_cast<pwm_config *>(config->device_config.data());
    auto timer = device_resource::get_timer_resource(pwm_conf->timer_conf);

    if (timer == nullptr || !timer->is_valid()) {
        Logger::log(LogLevel::Warning, "Couldn't create ledc driver or timer wasn't valid");
        return std::nullopt;
    }

    auto channel = device_resource::get_led_channel();

    if (channel == nullptr) {
        Logger::log(LogLevel::Warning, "Couldn't create channel");
        return std::nullopt;
    }

    auto gpio = device_resource::get_gpio_resource(static_cast<gpio_num_t>(pwm_conf->gpio_num), gpio_purpose::gpio);

    if (gpio == nullptr) {
        Logger::log(LogLevel::Warning, "Couldn't get gpio pin");
        return std::nullopt;
    }

    // TODO: do this differently, in led_channel 
    ledc_channel_config_t ledc_channel{};
    ledc_channel.gpio_num = gpio->gpio_num();
    ledc_channel.speed_mode = timer->timer_conf().freq_hz > 1000 ? LEDC_HIGH_SPEED_MODE : LEDC_LOW_SPEED_MODE;
    ledc_channel.channel = channel->channel_num();
    pwm_conf->channel = ledc_channel.channel;
    ledc_channel.timer_sel = timer->timer_num();
    ledc_channel.duty = 0;
    ledc_channel.hpoint = 0; 
    ledc_channel.intr_type = ledc_intr_type_t::LEDC_INTR_DISABLE;

    auto result = ledc_channel_config(&ledc_channel);

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't create ledc driver");
        return std::nullopt;
    }

    Logger::log(LogLevel::Info, "create_driver added new device");

    return std::make_optional(PwmDriver{config, timer, gpio, channel});
}

std::optional<PwmDriver> PwmDriver::create_driver(const std::string_view input, device_config &device_conf_out) {
    // Only prepare device_conf_out in this method and pass it along
    pwm_config new_conf{};
    int frequency = new_conf.timer_conf.frequency;
    int resolution = new_conf.timer_conf.resolution;
    int channel = new_conf.channel;
    int max_value = new_conf.max_value;
    int gpio_num = new_conf.gpio_num;
    bool fade = new_conf.fade;
    bool invert = new_conf.invert;

    json_scanf(input.data(), input.size(),
        "{ frequency : %d, resolution : %d, channel : %d, max_value : %d, gpio_num : %d, fade : %B, invert : %B}", 
        &frequency, &resolution, &channel, &max_value, &gpio_num, &fade, &invert);

    bool assign_result = true;
    assign_result &= check_assign(new_conf.timer_conf.frequency, frequency);
    assign_result &= check_assign(new_conf.timer_conf.resolution, resolution);
    assign_result &= check_assign(new_conf.channel, channel);
    assign_result &= check_assign(new_conf.max_value, max_value);
    assign_result &= check_assign(new_conf.gpio_num, gpio_num);
    new_conf.fade = static_cast<bool>(fade);
    new_conf.invert = static_cast<bool>(invert);

    if (!assign_result) {
        Logger::log(LogLevel::Error, "Some value(s) were out of range");
        return std::nullopt;
    }

    if (new_conf.timer_conf.frequency < 1000 ) {
        new_conf.timer_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    }

    std::memcpy(reinterpret_cast<pwm_config *>(device_conf_out.device_config.data()), &new_conf, sizeof(pwm_config));
    return create_driver(&device_conf_out);
}