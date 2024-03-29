#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <optional>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/dac.h"
#include "driver/i2c.h"
#include "hal/dac_types.h"
#include "hal/ledc_types.h"

enum struct gpio_purpose {
    bus, gpio
};

enum struct i2c_ports {
    zero = I2C_NUM_0, 
    one = I2C_NUM_1
};

class gpio_resource final {
    public:
        gpio_resource(const gpio_resource &) = delete;
        gpio_resource(gpio_resource &&);
        ~gpio_resource();

        gpio_resource &operator=(const gpio_resource &) = delete;
        gpio_resource &operator=(gpio_resource &&);

        void swap(gpio_resource &other);

        explicit operator gpio_num_t() const;
        gpio_num_t gpio_num() const;
        gpio_purpose purpose() const;

    private:
        gpio_resource(gpio_num_t gpio_num, gpio_purpose purpose);

        gpio_num_t m_gpio_num;
        gpio_purpose m_purpose;
        friend class device_resource;
};

void swap(gpio_resource &lhs, gpio_resource &rhs);

struct timer_config {
    ledc_mode_t speed_mode = ledc_mode_t::LEDC_HIGH_SPEED_MODE;
    ledc_timer_bit_t resolution = ledc_timer_bit_t::LEDC_TIMER_10_BIT;
    uint16_t frequency = 1000;
};

bool operator==(const timer_config &lhs, const timer_config &rhs);
bool operator!=(const timer_config &lhs, const timer_config &rhs);

class timer_resource final {
    public:
        timer_resource(const timer_resource &) = delete;
        timer_resource(timer_resource &&);
        ~timer_resource();

        timer_resource &operator=(const timer_resource &) = delete;
        timer_resource &operator=(timer_resource &&);

        void swap(timer_resource &other);

        bool has_conf(const timer_config &conf) const;
        ledc_timer_t timer_num() const;
        ledc_timer_config_t timer_conf() const;
        bool is_valid() const;
    private:
        timer_resource(ledc_timer_t timer_num, const timer_config &config);

        ledc_timer_t m_timer_num;
        timer_config m_config;
        bool m_valid;

        friend class device_resource;
};

void swap(timer_resource &lhs, timer_resource &rhs);

class led_channel final {
    public:
        led_channel(const led_channel &) = delete;
        led_channel(led_channel &&);
        ~led_channel();

        led_channel &operator=(const led_channel &) = delete;
        led_channel &operator=(led_channel &&);

        void swap(led_channel &other);

        ledc_channel_t channel_num() const;
    private:
        led_channel(ledc_channel_t channel);

        ledc_channel_t m_channel;

        friend class device_resource;
};

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

// TODO: implement
class i2c_resource final {
    public:
        i2c_resource(const i2c_resource &) = delete;
        i2c_resource(dac_resource &&);
        ~i2c_resource();

        i2c_resource &operator=(const i2c_resource &) = delete;
        i2c_resource &operator=(i2c_resource &&);

        void swap(i2c_resource &other);

        i2c_port_t channel_num() const;
    private:
        i2c_resource(i2c_resource channel, std::shared_ptr<gpio_resource> &sdaPin, std::shared_ptr<gpio_resource> &sclPin);

        i2c_port_t m_i2c_num;
        std::shared_ptr<gpio_resource> m_sdaPin;
        std::shared_ptr<gpio_resource> m_sclPin;

        friend class device_resource;
};

void swap(led_channel &lhs, led_channel &rhs);

class device_resource final {
    public:
        static std::shared_ptr<gpio_resource> get_gpio_resource(gpio_num_t pin, gpio_purpose mode);
        static std::shared_ptr<timer_resource> get_timer_resource(const timer_config &config);
        static std::shared_ptr<dac_resource> get_dac_resource(dac_channel_t channel);
        static std::shared_ptr<led_channel> get_led_channel();
        // TODO: implement
        static std::shared_ptr<i2c_resource> get_i2c_port(i2c_port_t port, i2c_mode_t mode, gpio_num_t sda, gpio_num_t scl);

    private:
        // TODO: replace with weak_ptr
        // TODO: remove non useable gpios
        static inline std::array _gpios{
            // std::make_pair(gpio_num_t::GPIO_NUM_1, std::shared_ptr<gpio_resource>(nullptr)),
            // std::make_pair(gpio_num_t::GPIO_NUM_2, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_3, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_4, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_5, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_6, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_7, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_8, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_9, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_10, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_11, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_12, std::shared_ptr<gpio_resource>(nullptr)),
            // std::make_pair(gpio_num_t::GPIO_NUM_13, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_14, std::shared_ptr<gpio_resource>(nullptr)),
            // std::make_pair(gpio_num_t::GPIO_NUM_15, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_16, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_17, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_18, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_19, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_20, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_21, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_22, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_23, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_25, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_26, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_27, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_28, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_29, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_30, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_31, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_32, std::shared_ptr<gpio_resource>(nullptr)),
            std::make_pair(gpio_num_t::GPIO_NUM_33, std::shared_ptr<gpio_resource>(nullptr)),
        };

        static inline std::array _dacs {
            std::make_pair(dac_channel_t::DAC_CHANNEL_1, std::shared_ptr<dac_resource>(nullptr)),
            std::make_pair(dac_channel_t::DAC_CHANNEL_2, std::shared_ptr<dac_resource>(nullptr))
        };

        static inline std::array _timers {
            std::make_pair(ledc_timer_t::LEDC_TIMER_0, std::shared_ptr<timer_resource>(nullptr)),
            std::make_pair(ledc_timer_t::LEDC_TIMER_1, std::shared_ptr<timer_resource>(nullptr)),
            std::make_pair(ledc_timer_t::LEDC_TIMER_2, std::shared_ptr<timer_resource>(nullptr)),
            std::make_pair(ledc_timer_t::LEDC_TIMER_3, std::shared_ptr<timer_resource>(nullptr)),
        };

        static inline std::array _channels {
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_0, std::shared_ptr<led_channel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_1, std::shared_ptr<led_channel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_2, std::shared_ptr<led_channel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_3, std::shared_ptr<led_channel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_4, std::shared_ptr<led_channel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_5, std::shared_ptr<led_channel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_6, std::shared_ptr<led_channel>(nullptr)),
            std::make_pair(ledc_channel_t::LEDC_CHANNEL_7, std::shared_ptr<led_channel>(nullptr)),
        };

        static inline std::array _i2c_ports {
            std::make_pair(i2c_ports::zero, std::shared_ptr<i2c_resource>(nullptr)),
            std::make_pair(i2c_ports::one, std::shared_ptr<i2c_resource>(nullptr))
        };

        static inline std::recursive_mutex _instance_mutex;
};