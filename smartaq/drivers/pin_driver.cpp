#include "pin_driver.h"

#include <algorithm>
#include <cstring>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "drivers/device_resource.h"
#include "drivers/device_types.h"
#include "frozen.h"
#include "hal/gpio_types.h"
#include "hal/ledc_types.h"
#include "utils/logger.h"
#include "utils/check_assign.h"

PinDriver::PinDriver(const DeviceConfig*conf, std::shared_ptr<TimerResource> timer, std::shared_ptr<GpioResource> gpio, std::shared_ptr<LedChannel> channel)
: m_conf(conf), m_timer(timer), m_gpio(gpio), m_channel(channel) { }

DeviceOperationResult PinDriver::write_value(std::string_view what, const DeviceValues &value) {
    // TODO: should the last pin state be safed ?
    auto *pinConf = m_conf->accessConfig<PinConfig>();
    esp_err_t result = ESP_FAIL;
    
    if (pinConf == nullptr || pinConf->type == PinType::Input) {
        return DeviceOperationResult::not_supported;
    }

    if (pinConf->type == PinType::Pwm) {
        decltype(value.generic_pwm()) pwmValue = 0;

        if (value.generic_pwm().has_value()) {
            pwmValue = *value.generic_pwm();
        } else if (value.percentage().has_value()) {
            static constexpr auto MaxPercentageValue = 100;
            const auto clampedPercentage = std::clamp<decltype(value.percentage())::value_type>(*value.percentage(), 0, MaxPercentageValue);
            pwmValue = static_cast<decltype(pwmValue)::value_type>((pinConf->max_value / MaxPercentageValue) * clampedPercentage);
        }

        if (!pwmValue.has_value()) {
            return DeviceOperationResult::not_supported;
        }

        if (!pinConf->invert) {
            m_current_value = std::clamp<uint16_t>(*pwmValue, 0, pinConf->max_value);
        } else {
            m_current_value = pinConf->max_value - std::clamp<uint16_t>(*pwmValue, 0, pinConf->max_value);
        }

        result = ESP_OK;
        if (!pinConf->fade) {
            if (ledc_get_duty(pinConf->timer_conf.speed_mode, pinConf->channel) != m_current_value) {
                result = ledc_set_duty(pinConf->timer_conf.speed_mode, pinConf->channel, m_current_value);
                result = ledc_update_duty(pinConf->timer_conf.speed_mode, pinConf->channel);
                Logger::log(LogLevel::Info, "Setting new value %d", m_current_value);
            } else {
                Logger::log(LogLevel::Info, "New Value is the same as the old one %d", m_current_value);
            }
        } else {
            // Maybe do the fading in a dedicated thread, and block that thread ?
            if (ledc_get_duty(pinConf->timer_conf.speed_mode, pinConf->channel) != m_current_value) {
                result = ledc_set_fade_time_and_start(pinConf->timer_conf.speed_mode, pinConf->channel, m_current_value, 1000, ledc_fade_mode_t::LEDC_FADE_NO_WAIT);
                Logger::log(LogLevel::Info, "Setting new value %d", m_current_value);
            } else {
                Logger::log(LogLevel::Info, "New Value is the same as the old one %d", m_current_value);
            }
        }
    } else if (pinConf->type == PinType::Output) {
        std::optional<uint32_t> level{0u};

        if (value.enable().has_value()) {
            level = static_cast<uint32_t>(static_cast<bool>(*value.enable()));
        } else if (value.percentage().has_value()) {
            level = static_cast<bool>(*value.percentage());
        }

        if (!level.has_value()) {
            return DeviceOperationResult::failure;
        }

        result = gpio_set_level(static_cast<gpio_num_t>(pinConf->gpio_num), static_cast<bool>(1 * static_cast<uint32_t>(pinConf->invert) - *level));

    }

    return result == ESP_OK ? DeviceOperationResult::ok : DeviceOperationResult::failure;
}

// TODO: maybe just return current value, for the other types
DeviceOperationResult PinDriver::read_value(std::string_view what, DeviceValues &values) const {
    auto *pinConf = m_conf->accessConfig<PinConfig>();

    if (pinConf->type == PinType::Input) {
        return DeviceOperationResult::not_supported;
    }

    values.enable(gpio_get_level(static_cast<gpio_num_t>(pinConf->gpio_num)));

    return DeviceOperationResult::ok ;
}

DeviceOperationResult PinDriver::call_device_action(DeviceConfig*conf, const std::string_view &action, const std::string_view &json) {
    return DeviceOperationResult::not_supported;
}


DeviceOperationResult PinDriver::update_runtime_data() {
    return DeviceOperationResult::ok;
}

DeviceOperationResult PinDriver::get_info(char *output_buffer, size_t output_buffer_len) const {
    json_out out = JSON_OUT_BUF(output_buffer, output_buffer_len);
    json_printf(&out, "{}");
    return DeviceOperationResult::ok;
}

std::optional<PinDriver> PinDriver::create_driver(const DeviceConfig *config) {
    static std::once_flag init_fading{};
    std::call_once(init_fading, [](){ 
        Logger::log(LogLevel::Info, "Initialized fading");
        ledc_fade_func_install(0); 
    });

    auto *pinConf = config->accessConfig<PinConfig>();

    if (pinConf->type == PinType::Invalid) {
        Logger::log(LogLevel::Warning, "Pin type is invalid");
        return std::nullopt;
    }

    auto gpio = DeviceResource::get_gpio_resource(static_cast<gpio_num_t>(pinConf->gpio_num), GpioPurpose::gpio);

    if (gpio == nullptr) {
        Logger::log(LogLevel::Warning, "Couldn't get gpio pin");
        return std::nullopt;
    }

    // TODO: check what open drain means
    const gpio_mode_t direction = pinConf->type == PinType::Input ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT;
    auto result = gpio_set_direction(gpio->gpio_num(), direction);

    if (result != ESP_OK) {
        Logger::log(LogLevel::Warning, "Couldn't set gpio direction");
        return std::nullopt;
    }

    if (pinConf->type == PinType::Pwm) {
        auto timer = DeviceResource::get_timer_resource(pinConf->timer_conf);

        if (timer == nullptr || !timer->isValid()) {
            Logger::log(LogLevel::Warning, "Couldn't create ledc driver or timer wasn't valid");
            return std::nullopt;
        }

        auto channel = DeviceResource::get_led_channel();

        if (channel == nullptr) {
            Logger::log(LogLevel::Warning, "Couldn't create channel");
            return std::nullopt;
        }

        // TODO: do this differently, in led_channel 
        ledc_channel_config_t ledc_channel{};
        ledc_channel.gpio_num = gpio->gpio_num();
        // TODO: enable for older esp32
        // ledc_channel.speed_mode = timer->timer_conf().freq_hz > 1000 ? LEDC_LOW_SPEED_MODE : LEDC_LOW_SPEED_MODE;
        ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
        ledc_channel.channel = channel->channelNum();
        pinConf->channel = ledc_channel.channel;
        ledc_channel.timer_sel = timer->timerNum();
        ledc_channel.duty = 0;
        ledc_channel.hpoint = 0; 
        ledc_channel.intr_type = ledc_intr_type_t::LEDC_INTR_DISABLE;

        auto result = ledc_channel_config(&ledc_channel);

        if (result != ESP_OK) {
            Logger::log(LogLevel::Warning, "Couldn't create ledc driver");
            return std::nullopt;
        }

        return std::make_optional(PinDriver{config, timer, gpio, channel});
    } 

    Logger::log(LogLevel::Info, "create_driver added new device");

    return std::make_optional(PinDriver{config, nullptr, gpio, nullptr});
}

std::optional<PinDriver> PinDriver::create_driver(const std::string_view input, DeviceConfig&device_conf_out) {
    // Only prepare device_conf_out in this method and pass it along
    PinConfig newConf{};
    uint16_t frequency = newConf.timer_conf.frequency;
    uint16_t resolution = newConf.timer_conf.resolution;
    uint16_t channel = newConf.channel;
    uint16_t max_value = newConf.max_value;
    uint16_t gpio_num = newConf.gpio_num;
    bool fade = newConf.fade;
    bool invert = newConf.invert;

    json_scanf(input.data(), input.size(),
        "{ type : %M, frequency : %u, resolution : %u, channel : %u, max_value : %u, gpio_num : %u, fade : %B, invert : %B}",
        json_scanf_single<PinType>, &newConf.type,
        &frequency, &resolution, &channel, &max_value, &gpio_num, &fade, &invert);

    bool assign_result = true;
    assign_result &= checkAssign(newConf.timer_conf.frequency, frequency);
    assign_result &= checkAssign(newConf.timer_conf.resolution, resolution);
    assign_result &= checkAssign(newConf.channel, channel);
    assign_result &= checkAssign(newConf.max_value, max_value);
    assign_result &= checkAssign(newConf.gpio_num, gpio_num);
    newConf.fade = static_cast<bool>(fade);
    newConf.invert = static_cast<bool>(invert);

    if (!assign_result) {
        Logger::log(LogLevel::Error, "Some value(s) were out of range");
        return std::nullopt;
    }

    if (newConf.timer_conf.frequency < 1000 ) {
        newConf.timer_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    }

    std::memcpy(device_conf_out.device_config.data(), &newConf, sizeof(PinConfig));
    return create_driver(&device_conf_out);
}