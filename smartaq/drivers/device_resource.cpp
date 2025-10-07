#include "build_config.h"

#include "device_resource.h"

#include <algorithm>

#include "hal/dac_types.h"
#include "hal/gpio_types.h"

bool operator==(const I2cPorts& lhs, const i2c_port_t& rhs)
{
    return std::to_underlying(lhs) == rhs;
}

bool operator!=(const I2cPorts& lhs, const i2c_port_t& rhs)
{
    return !(lhs == rhs);
}

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

I2cResource::I2cResource(i2c_port_t channel, i2c_config_t config, std::shared_ptr<GpioResource> sdaPin, std::shared_ptr<GpioResource> sclPin)
    : m_i2c_num(channel), m_i2c_config(config), m_sdaPin(sdaPin), m_sclPin(sclPin)
{

}
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

std::shared_ptr<I2cResource> DeviceResource::createI2CPort(i2c_port_t port, i2c_mode_t mode,
                                                             std::shared_ptr<GpioResource> sda,
                                                             std::shared_ptr<GpioResource> scl)
{
    if (mode != I2C_MODE_MASTER)
    {
        Logger::log(LogLevel::Warning, "I2C mode %d is not supported", mode);
        return nullptr;
    }

    if (sda == nullptr || scl == nullptr)
    {
        Logger::log(LogLevel::Debug, "SDA or SCL pin is not configured");
        return nullptr;
    }

    // TODO: Add a noinit flag
    // 1. Configure I2C with pull-ups
    i2c_config_t conf = {};
    /*conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda->gpio_num();
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl->gpio_num();
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100'000;

    Logger::log(LogLevel::Debug, "Configuring I2C port %d ...", port);

    esp_err_t result = i2c_param_config(port, &conf);

    if (result != ESP_OK)
    {
        Logger::log(LogLevel::Warning, "Couldn't configure I2C port %d", port);
        return nullptr;
    }

    Logger::log(LogLevel::Debug, "Installing I2C driver on port %d ...", port);
    */
    // result = i2c_driver_install(port, conf.mode,
    //                             0,
    //                             0, 0);

    // if (result != ESP_OK)
    // {
    //     Logger::log(LogLevel::Warning, "Couldn't install I2C port %d", port);
    //     return nullptr;
    // }

    Logger::log(LogLevel::Debug, "I2C port %d created ...", port);

    return std::shared_ptr<I2cResource>{new I2cResource(port, conf, std::move(sda), std::move(scl))};
}

std::shared_ptr<I2cResource> DeviceResource::get_i2c_port(i2c_port_t port, i2c_mode_t mode, gpio_num_t sda,
    gpio_num_t scl)
{
    auto foundResource = std::ranges::find_if(_i2c_ports, [port](auto &current_entry)
    {
        return current_entry.first == port;
    });

    if (foundResource == _i2c_ports.end() || foundResource == nullptr)
    {
        Logger::log(LogLevel::Warning, "I2C port %d not found", port);
        return nullptr;
    }

    if (foundResource->second != nullptr)
    {
        Logger::log(LogLevel::Info, "Configured I2C port %d found", port);
        return foundResource->second;
    }

    Logger::log(LogLevel::Debug, "Acquiring gpio ports for bus sda: %u scl: %u", sda, scl);

    auto sdaPin = DeviceResource::get_gpio_resource(sda, GpioPurpose::bus);
    auto sclPin = DeviceResource::get_gpio_resource(scl, GpioPurpose::bus);

    Logger::log(LogLevel::Debug, "Creating I2C port %d ...", port);

    auto createdPort = createI2CPort(port, mode, std::move(sdaPin), std::move(sclPin));
    foundResource->second = std::move(createdPort);
    return foundResource->second;
}
