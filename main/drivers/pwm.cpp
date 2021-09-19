#include "pwm.h"

#include <mutex>
#include <cstring>

#include "frozen.h"
#include "esp_log.h"
#include "utils/utils.h"

PwmDriver::PwmDriver(const device_config *conf, std::shared_ptr<timer_resource> timer, std::shared_ptr<gpio_resource> gpio, std::shared_ptr<led_channel> channel) 
: m_conf(conf), m_timer(timer), m_gpio(gpio), m_channel(channel) { }

DeviceOperationResult PwmDriver::write_value(const device_values &values) {
    auto *pwm_conf = reinterpret_cast<pwm_config *>(m_conf->device_config.data());
    esp_err_t result = ESP_FAIL;

    // TODO: incorperate percentage value
    if (!values.generic_pwm.has_value()) {
        return DeviceOperationResult::not_supported;
    }

    pwm_conf->current_value = *values.generic_pwm;
    ESP_LOGI("pwm_driver", "Setting new value %d", pwm_conf->current_value);

    if (!pwm_conf->fade) {
        result = ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, pwm_conf->channel, pwm_conf->current_value, 0);
    } else {
        // Maybe do the fading in a dedicated thread, and block that thread ?
        result = ledc_set_fade_time_and_start(LEDC_HIGH_SPEED_MODE, pwm_conf->channel, pwm_conf->current_value, 1000, ledc_fade_mode_t::LEDC_FADE_NO_WAIT);
    }

    return result == ESP_OK ? DeviceOperationResult::ok : DeviceOperationResult::failure;
}

// TODO: implement both
DeviceOperationResult PwmDriver::read_value(device_values &values) const {
    return DeviceOperationResult::not_supported;
}

DeviceOperationResult PwmDriver::write_device_options(const char *json_input, size_t input_len) {
    return DeviceOperationResult::ok;
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
        ESP_LOGI("pwm_driver", "Initialized fading");
        ledc_fade_func_install(0); 
    });

    auto *pwm_conf = reinterpret_cast<pwm_config *>(config->device_config.data());
    auto timer = device_resource::get_timer_resource(pwm_conf->timer_conf);

    if (timer == nullptr || !timer->is_valid()) {
        ESP_LOGW("pwm_driver", "Couldn't create ledc driver or timer wasn't valid");
        return std::nullopt;
    }

    auto channel = device_resource::get_led_channel();

    if (channel == nullptr) {
        ESP_LOGW("pwm_driver", "Couldn't create channel");
        return std::nullopt;
    }

    auto gpio = device_resource::get_gpio_resource(static_cast<gpio_num_t>(pwm_conf->gpio_num), gpio_purpose::gpio);

    if (gpio == nullptr) {
        ESP_LOGW("pwm_driver", "Couldn't get gpio pin");
        return std::nullopt;
    }

    // TODO: do this differently, in led_channel 
    ledc_channel_config_t ledc_channel{};
    ledc_channel.gpio_num = gpio->gpio_num();
    ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_channel.channel = channel->channel_num();
    ledc_channel.timer_sel = timer->timer_num();
    ledc_channel.duty = pwm_conf->current_value;
    ledc_channel.hpoint = 0;
    ledc_channel.intr_type = ledc_intr_type_t::LEDC_INTR_DISABLE;

    auto result = ledc_channel_config(&ledc_channel);

    if (result != ESP_OK) {
        ESP_LOGW("pwm_driver", "Couldn't create ledc driver");
        return std::nullopt;
    }

    ESP_LOGI("pwm_driver", "create_driver added new device");

    return std::make_optional(PwmDriver{config, timer, gpio, channel});
}

std::optional<PwmDriver> PwmDriver::create_driver(const std::string_view input, device_config &device_conf_out) {
    // Only prepare device_conf_out in this method and pass it along
    pwm_config new_conf{};
    int frequency = new_conf.timer_conf.frequency;
    int resolution = new_conf.timer_conf.resolution;
    int channel = new_conf.channel;
    int max_value = new_conf.max_value;
    int current_value = new_conf.current_value;
    int gpio_num = new_conf.gpio_num;
    bool fade = new_conf.fade;

    json_scanf(input.data(), input.size(),
        "{ frequency : %d, resolution : %d, channel : %d, max_value : %d, current_value : %d, gpio_num : %d, fade : %B}", 
        &frequency, &resolution, &channel, &max_value, &current_value, &gpio_num, &fade);

    bool assign_result = true;
    assign_result &= check_assign(new_conf.timer_conf.frequency, frequency);
    assign_result &= check_assign(new_conf.timer_conf.resolution, resolution);
    assign_result &= check_assign(new_conf.channel, channel);
    assign_result &= check_assign(new_conf.max_value, max_value);
    assign_result &= check_assign(new_conf.current_value, current_value);
    assign_result &= check_assign(new_conf.gpio_num, gpio_num);
    new_conf.fade = static_cast<bool>(fade);

    if (!assign_result) {
        ESP_LOGW("pwm_driver", "Some value(s) were out of range");
        return std::nullopt;
    }

    std::memcpy(reinterpret_cast<pwm_config *>(device_conf_out.device_config.data()), &new_conf, sizeof(pwm_config));
    return create_driver(&device_conf_out);
}