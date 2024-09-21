#include "build_config.h"

#include "device_resource.h"

#include <algorithm>

#include "hal/dac_types.h"
#include "hal/gpio_types.h"

GpioResource::GpioResource(gpio_num_t gpio_num, GpioPurpose purpose) : m_gpio_num(gpio_num), m_purpose(purpose) { }

// TODO: reset gpio
GpioResource::~GpioResource() {}

GpioResource::GpioResource(GpioResource &&other) : m_gpio_num(other.m_gpio_num) {
    other.m_gpio_num = gpio_num_t::GPIO_NUM_MAX;
}

GpioResource &GpioResource::operator=(GpioResource &&other) {
    using std::swap;

    swap(m_gpio_num, other.m_gpio_num);

    return *this;
}

void GpioResource::swap(GpioResource &other) {
    using std::swap;

    swap(m_gpio_num, other.m_gpio_num);
}

void swap(GpioResource &lhs, GpioResource &rhs) {
    lhs.swap(rhs);
}

GpioResource::operator gpio_num_t() const {
    return m_gpio_num;
}

gpio_num_t GpioResource::gpio_num() const {
    return m_gpio_num;
}

GpioPurpose GpioResource::purpose() const {
    return m_purpose;
}

TimerResource::TimerResource(ledc_timer_t timer_num, const TimerConfig &config) : m_timer_num(timer_num), m_config(config) {
    ledc_timer_config_t tconfig{
        .speed_mode = m_config.speed_mode,
        .duty_resolution = m_config.resolution,
        .timer_num = m_timer_num,
        .freq_hz = m_config.frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };

    m_valid = ledc_timer_config(&tconfig) == ESP_OK;
}

TimerResource::TimerResource(TimerResource &&other) : m_timer_num(other.m_timer_num), m_config(other.m_config) {
    other.m_timer_num = ledc_timer_t::LEDC_TIMER_MAX; 
}

// TODO: stop timer
TimerResource::~TimerResource() { }

TimerResource &TimerResource::operator=(TimerResource &&other) {
    using std::swap;

    swap(m_timer_num, other.m_timer_num);
    swap(m_config, other.m_config);

    return *this;
}

void TimerResource::swap(TimerResource &other) {
    using std::swap;

    swap(m_timer_num, other.m_timer_num);
    swap(m_config, other.m_config);
}

bool TimerResource::hasConf(const TimerConfig &conf) const {
    return m_config == conf;
}

ledc_timer_t TimerResource::timerNum() const {
    return m_timer_num;
}

ledc_timer_config_t TimerResource::timerConf() const {
    return ledc_timer_config_t {
        .speed_mode = m_config.speed_mode,
        .duty_resolution = m_config.resolution,
        .timer_num = m_timer_num,
        .freq_hz = m_config.frequency,
        .clk_cfg = LEDC_AUTO_CLK
    };
}

bool TimerResource::isValid() const {
    return m_valid;
}

void swap(TimerResource &lhs, TimerResource &rhs) {
    lhs.swap(rhs);
}

LedChannel::LedChannel(ledc_channel_t channel) : m_channel(channel) { }

LedChannel::LedChannel(LedChannel &&other) : m_channel(other.m_channel) { other.m_channel = ledc_channel_t::LEDC_CHANNEL_MAX; }

// TODO: Delete LedChannel
LedChannel::~LedChannel() { }

ledc_channel_t LedChannel::channelNum() const {
    return m_channel;
}

LedChannel &LedChannel::operator=(LedChannel &&other) {
    using std::swap;

    swap(m_channel, other.m_channel);

    return *this;
}

std::shared_ptr<GpioResource> DeviceResource::get_gpio_resource(gpio_num_t pin, GpioPurpose mode) {
    std::lock_guard instance_guard{_instance_mutex};

    auto found_resource = std::ranges::find_if(_gpios, [pin](auto &current_entry) {
        return current_entry.first == pin;
    });

    if (found_resource == _gpios.cend()) {
        return nullptr;
    }

    if (found_resource->second == nullptr) {
        found_resource->second = std::shared_ptr<GpioResource>(new GpioResource(pin, mode));
    }

    // GPIO mode can't be shared, maybe input only
    if (found_resource->second->purpose() != mode 
        || (found_resource->second->purpose() == GpioPurpose::gpio && found_resource->second.use_count() > 1)) {
        return nullptr;
    }

    return found_resource->second;
}

#ifdef ENABLE_DAC_DRIVER

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

#endif

// TODO: fix search for free resources
std::shared_ptr<TimerResource> DeviceResource::get_timer_resource(const TimerConfig &config) {
    std::lock_guard instance_guard{_instance_mutex};

    auto found_resource = std::find_if(_timers.begin(), _timers.end(), [&config](auto &current_entry) {
        return current_entry.second == nullptr || current_entry.second->hasConf(config);
    });

    if (found_resource == _timers.cend()) {
        return nullptr;
    }

    if (found_resource->second == nullptr) {
        found_resource->second = std::shared_ptr<TimerResource>(new TimerResource(found_resource->first, config));
    }

    return found_resource->second;
}

std::shared_ptr<LedChannel> DeviceResource::get_led_channel() {
    std::lock_guard instance_guard{_instance_mutex};

    auto found_resource = std::find_if(_channels.begin(), _channels.end(), [](auto &current_entry) {
        return current_entry.second == nullptr;
    });

    if (found_resource == _channels.cend()) {
        return nullptr;
    }

    if (found_resource->second == nullptr) {
        found_resource->second = std::shared_ptr<LedChannel>(new LedChannel(found_resource->first));
    }

    return found_resource->second;
}