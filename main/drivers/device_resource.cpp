#include "device_resource.h"

#include <algorithm>

#include "driver/dac_common.h"
#include "hal/dac_types.h"
#include "hal/gpio_types.h"

#include "build_config.h"

gpio_resource::gpio_resource(gpio_num_t gpio_num, gpio_purpose purpose) : m_gpio_num(gpio_num), m_purpose(purpose) { }

// TODO: reset gpio
gpio_resource::~gpio_resource() {}

gpio_resource::gpio_resource(gpio_resource &&other) : m_gpio_num(other.m_gpio_num) { 
    other.m_gpio_num = gpio_num_t::GPIO_NUM_MAX;
}

gpio_resource &gpio_resource::operator=(gpio_resource &&other) {
    using std::swap;

    swap(m_gpio_num, other.m_gpio_num);

    return *this;
}

void gpio_resource::swap(gpio_resource &other) {
    using std::swap;

    swap(m_gpio_num, other.m_gpio_num);
}

void swap(gpio_resource &lhs, gpio_resource &rhs) {
    lhs.swap(rhs);
}

gpio_resource::operator gpio_num_t() const {
    return m_gpio_num;
}

gpio_num_t gpio_resource::gpio_num() const {
    return m_gpio_num;
}

gpio_purpose gpio_resource::purpose() const {
    return m_purpose;
}

timer_resource::timer_resource(ledc_timer_t timer_num, const timer_config &config) : m_timer_num(timer_num), m_config(config) {
    ledc_timer_config_t tconfig{
        .speed_mode = m_config.speed_mode,
        .duty_resolution = m_config.resolution,
        .timer_num = m_timer_num,
        .freq_hz = m_config.frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };

    m_valid = ledc_timer_config(&tconfig) == ESP_OK;
}

timer_resource::timer_resource(timer_resource &&other) : m_timer_num(other.m_timer_num), m_config(other.m_config) { 
    other.m_timer_num = ledc_timer_t::LEDC_TIMER_MAX; 
}

// TODO: stop timer
timer_resource::~timer_resource() { }

timer_resource &timer_resource::operator=(timer_resource &&other) {
    using std::swap;

    swap(m_timer_num, other.m_timer_num);
    swap(m_config, other.m_config);

    return *this;
}

void timer_resource::swap(timer_resource &other) {
    using std::swap;

    swap(m_timer_num, other.m_timer_num);
    swap(m_config, other.m_config);
}

bool timer_resource::has_conf(const timer_config &conf) const {
    return m_config == conf;
}

ledc_timer_t timer_resource::timer_num() const {
    return m_timer_num;
}

ledc_timer_config_t timer_resource::timer_conf() const {
    return ledc_timer_config_t {
        .speed_mode = m_config.speed_mode,
        .duty_resolution = m_config.resolution,
        .timer_num = m_timer_num,
        .freq_hz = m_config.frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };
}

bool timer_resource::is_valid() const {
    return m_valid;
}

void swap(timer_resource &lhs, timer_resource &rhs) {
    lhs.swap(rhs);
}

bool operator==(const timer_config &lhs, const timer_config &rhs) {
    return (lhs.frequency == rhs.frequency) 
    && (lhs.resolution == rhs.resolution)
    && (lhs.speed_mode == rhs.speed_mode);
}

bool operator!=(const timer_config &lhs, const timer_config &rhs) {
    return !(lhs == rhs);
}

led_channel::led_channel(ledc_channel_t channel) : m_channel(channel) { }

led_channel::led_channel(led_channel &&other) : m_channel(other.m_channel) { other.m_channel = ledc_channel_t::LEDC_CHANNEL_MAX; }

led_channel::~led_channel() { }

ledc_channel_t led_channel::channel_num() const {
    return m_channel;
}

led_channel &led_channel::operator=(led_channel &&other) {
    using std::swap;

    swap(m_channel, other.m_channel);

    return *this;
}

dac_resource::dac_resource(dac_channel_t channel, std::shared_ptr<gpio_resource> &pin_resource) : m_channel_num(channel), m_pin_resource(pin_resource) { }

dac_resource::dac_resource(dac_resource &&other) : m_channel_num(other.m_channel_num), m_pin_resource(other.m_pin_resource) {
    other.m_channel_num = dac_channel_t::DAC_CHANNEL_MAX;
    other.m_pin_resource = nullptr;
}

dac_resource &dac_resource::operator=(dac_resource &&other) {

    using std::swap;

    swap(m_channel_num, other.m_channel_num);
    swap(m_pin_resource, other.m_pin_resource);

    other.m_channel_num = dac_channel_t::DAC_CHANNEL_MAX;
    other.m_pin_resource = nullptr;

    return *this;
}

dac_channel_t dac_resource::channel_num() const {
    return m_channel_num;
}

// TODO: reset dac
dac_resource::~dac_resource() { }

std::shared_ptr<gpio_resource> device_resource::get_gpio_resource(gpio_num_t pin, gpio_purpose mode) {
    std::lock_guard instance_guard{_instance_mutex};

    auto found_resource = std::find_if(_gpios.begin(), _gpios.end(), [pin](auto &current_entry) {
        return current_entry.first == pin;
    });

    if (found_resource == _gpios.cend()) {
        return nullptr;
    }

    if (found_resource->second == nullptr) {
        found_resource->second = std::shared_ptr<gpio_resource>(new gpio_resource(pin, mode));
    }

    // GPIO mode can't be shared, maybe input only
    if (found_resource->second->purpose() != mode 
        || (found_resource->second->purpose() == gpio_purpose::gpio && found_resource->second.use_count() > 1)) {
        return nullptr;
    }

    return found_resource->second;
}

std::shared_ptr<dac_resource> device_resource::get_dac_resource(dac_channel_t channel)  {
    std::lock_guard instance_guard{_instance_mutex};

    auto found_resource = std::find_if(_dacs.begin(), _dacs.end(), [channel](auto &current_entry) {
        return current_entry.first == channel;
    });

    if (found_resource == _dacs.end()) {
        return nullptr;
    }

    // Resource is already in use
    if (found_resource->second != nullptr && found_resource->second.use_count() > 1) {
        return nullptr;
    }

    // Resource was already in use, but can be used again
    if (found_resource->second != nullptr && found_resource->second.use_count() == 1) {
        return found_resource->second;
    }

    gpio_num_t used_pin = gpio_num_t::GPIO_NUM_0;
    esp_err_t result = dac_pad_get_io_num(channel, &used_pin);

    if (result != ESP_OK && used_pin != gpio_num_t::GPIO_NUM_0) {
        return nullptr;
    }

    auto gpio_resource = device_resource::get_gpio_resource(used_pin, gpio_purpose::gpio);

    if (gpio_resource == nullptr) {
        return nullptr;
    }

    found_resource->second = std::shared_ptr<dac_resource>(new dac_resource(found_resource->first, gpio_resource));

    return found_resource->second;
}

// TODO: fix search for free resources
std::shared_ptr<timer_resource> device_resource::get_timer_resource(const timer_config &config) {
    std::lock_guard instance_guard{_instance_mutex};

    auto found_resource = std::find_if(_timers.begin(), _timers.end(), [&config](auto &current_entry) {
        return current_entry.second == nullptr || current_entry.second->has_conf(config);
    });

    if (found_resource == _timers.cend()) {
        return nullptr;
    }

    if (found_resource->second == nullptr) {
        found_resource->second = std::shared_ptr<timer_resource>(new timer_resource(found_resource->first, config));
    }

    return found_resource->second;
}

std::shared_ptr<led_channel> device_resource::get_led_channel() {
    std::lock_guard instance_guard{_instance_mutex};

    auto found_resource = std::find_if(_channels.begin(), _channels.end(), [](auto &current_entry) {
        return current_entry.second == nullptr;
    });

    if (found_resource == _channels.cend()) {
        return nullptr;
    }

    if (found_resource->second == nullptr) {
        found_resource->second = std::shared_ptr<led_channel>(new led_channel(found_resource->first));
    }

    return found_resource->second;
}