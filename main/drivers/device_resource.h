#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <optional>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c.h"

#include "hal/ledc_types.h"
#ifdef ENABLE_DAC_DRIVER
#include "driver/dac.h"
#include "hal/dac_types.h"
#endif

enum struct GpioPurpose {
    bus, gpio
};

enum struct I2cPorts {
    zero = I2C_NUM_0, 
    one = I2C_NUM_1
};

class GpioResource final {
    public:
        GpioResource(const GpioResource &) = delete;
        GpioResource(GpioResource &&);
        ~GpioResource();

        GpioResource &operator=(const GpioResource &) = delete;
        GpioResource &operator=(GpioResource &&);

        void swap(GpioResource &other);

        explicit operator gpio_num_t() const;
        gpio_num_t gpio_num() const;
        GpioPurpose purpose() const;

    private:
        GpioResource(gpio_num_t gpio_num, GpioPurpose purpose);

        gpio_num_t m_gpio_num;
        GpioPurpose m_purpose;
        friend class DeviceResource;
};

void swap(GpioResource &lhs, GpioResource &rhs);

struct TimerConfig {
    ledc_mode_t speed_mode = ledc_mode_t::LEDC_LOW_SPEED_MODE;
    ledc_timer_bit_t resolution = ledc_timer_bit_t::LEDC_TIMER_10_BIT;
    uint16_t frequency = 1000;

    bool operator==(const TimerConfig &other) const noexcept = default;
};

class TimerResource final {
    public:
        TimerResource(const TimerResource &) = delete;
        TimerResource(TimerResource &&);
        ~TimerResource();

        TimerResource &operator=(const TimerResource &) = delete;
        TimerResource &operator=(TimerResource &&);

        void swap(TimerResource &other);

        [[nodiscard]] bool hasConf(const TimerConfig &conf) const;
        [[nodiscard]] ledc_timer_t timerNum() const;
        [[nodiscard]] ledc_timer_config_t timerConf() const;
        [[nodiscard]] bool isValid() const;
    private:
        TimerResource(ledc_timer_t timer_num, const TimerConfig &config);

        ledc_timer_t m_timer_num;
        TimerConfig m_config;
        bool m_valid;

        friend class DeviceResource;
};

void swap(TimerResource &lhs, TimerResource &rhs);

class LedChannel final {
    public:
        LedChannel(const LedChannel &) = delete;
        LedChannel(LedChannel &&);
        ~LedChannel();

        LedChannel &operator=(const LedChannel &) = delete;
        LedChannel &operator=(LedChannel &&);

        void swap(LedChannel &other);

        [[nodiscard]] ledc_channel_t channelNum() const;
    private:
        explicit LedChannel(ledc_channel_t channel);

        ledc_channel_t m_channel;

        friend class DeviceResource;
};

#ifdef ENABLE_DAC_DRIVER

class dac_resource final {
    public:
        dac_resource(const dac_resource &) = delete;
        dac_resource(dac_resource &&);
        ~dac_resource();

        dac_resource &operator=(const dac_resource &) = delete;
        dac_resource &operator=(dac_resource &&);

        void swap(dac_resource &other);

        dac_channel_t channel_num() const;
    private:
        dac_resource(dac_channel_t channel, std::shared_ptr<gpio_resource> &pin_resource);

        dac_channel_t m_channel_num;
        std::shared_ptr<gpio_resource> m_pin_resource;

        friend class device_resource;
};
#endif

// TODO: implement
class I2cResource final {
    public:
        I2cResource(const I2cResource &) = delete;
        I2cResource(I2cResource &&) = default;
        ~I2cResource() = default;

        I2cResource &operator=(const I2cResource &) = delete;
        I2cResource &operator=(I2cResource &&) = default;

        void swap(I2cResource &other);

        i2c_port_t channel_num() const;
    private:
        I2cResource(I2cResource channel, std::shared_ptr<GpioResource> &sdaPin, std::shared_ptr<GpioResource> &sclPin);

        i2c_port_t m_i2c_num;
        std::shared_ptr<GpioResource> m_sdaPin;
        std::shared_ptr<GpioResource> m_sclPin;

        friend class DeviceResource;
};

void swap(LedChannel &lhs, LedChannel &rhs);

class DeviceResource final {
    public:
        static std::shared_ptr<GpioResource> get_gpio_resource(gpio_num_t pin, GpioPurpose mode);
        static std::shared_ptr<TimerResource> get_timer_resource(const TimerConfig &config);
        #ifdef ENABLE_DAC_DRIVER
        static std::shared_ptr<dac_resource> get_dac_resource(dac_channel_t channel);
        #endif
        static std::shared_ptr<LedChannel> get_led_channel();
        // TODO: implement
        static std::shared_ptr<I2cResource> get_i2c_port(i2c_port_t port, i2c_mode_t mode, gpio_num_t sda, gpio_num_t scl);

    private:
        // TODO: replace with weak_ptr
        // TODO: remove non useable gpios
        static inline std::array _gpios{
            // std::make_pair(gpio_num_t::GPIO_NUM_1, std::shared_ptr<gpio_resource>(nullptr)),
            // std::make_pair(gpio_num_t::GPIO_NUM_2, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_3, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_4, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_5, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_6, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_7, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_8, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_9, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_10, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_11, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_12, std::shared_ptr<GpioResource>(nullptr)),
            // std::make_pair(gpio_num_t::GPIO_NUM_13, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_14, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_15, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_16, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_17, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_18, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_19, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_20, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_21, std::shared_ptr<GpioResource>(nullptr)),

            #ifndef CONFIG_IDF_TARGET_ESP32S3
            std::make_pair(gpio_num_t::GPIO_NUM_22, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_23, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_25, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_32, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_33, std::shared_ptr<gpio_resource>(nullptr)),
            #endif

            std::make_pair(gpio_num_t::GPIO_NUM_26, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_27, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_28, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_29, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_30, std::shared_ptr<GpioResource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_31, std::shared_ptr<GpioResource>(nullptr)),
        };

        #ifdef ENABLE_DAC_DRIVER
        static inline std::array _dacs {
            std::make_pair(dac_channel_t::DAC_CHANNEL_1, std::shared_ptr<dac_resource>(nullptr)),
            std::make_pair(dac_channel_t::DAC_CHANNEL_2, std::shared_ptr<dac_resource>(nullptr))
        };
        #endif

        static inline std::array _timers {
            std::make_pair(ledc_timer_t::LEDC_TIMER_0, std::shared_ptr<TimerResource>(nullptr)),
            std::make_pair(ledc_timer_t::LEDC_TIMER_1, std::shared_ptr<TimerResource>(nullptr)),
            std::make_pair(ledc_timer_t::LEDC_TIMER_2, std::shared_ptr<TimerResource>(nullptr)),
            std::make_pair(ledc_timer_t::LEDC_TIMER_3, std::shared_ptr<TimerResource>(nullptr)),
        };

        static inline std::array _channels {
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_0, std::shared_ptr<LedChannel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_1, std::shared_ptr<LedChannel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_2, std::shared_ptr<LedChannel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_3, std::shared_ptr<LedChannel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_4, std::shared_ptr<LedChannel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_5, std::shared_ptr<LedChannel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_6, std::shared_ptr<LedChannel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_7, std::shared_ptr<LedChannel>(nullptr)),
        };

        static inline std::array _i2c_ports {
            std::make_pair(I2cPorts::zero, std::shared_ptr<I2cResource>(nullptr)),
            std::make_pair(I2cPorts::one, std::shared_ptr<I2cResource>(nullptr))
        };

        static inline std::recursive_mutex _instance_mutex;
};