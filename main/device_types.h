#pragma once

#include <array>
#include <optional>

#include "smartqua_config.h"
#include "driver/gpio.h"

enum struct device_read_result {
    ok, not_supported, failure
};

enum struct device_write_result {
    ok, not_supported, failure
};

struct device_config { 
    std::array<gpio_num_t, max_pins_for_device> pins;
    std::array<char, name_length> device_driver;
    std::array<char, 256> device_config;
};

struct device_values {
    std::optional<float> temperature;
    std::optional<float> ph;
    std::optional<float> humidity;
    std::optional<float> voltage;
    std::optional<float> ampere;
    std::optional<float> watt;
    std::optional<int16_t> tds;
    std::optional<int16_t> analog;
    std::optional<int16_t> generic_pwm;
    std::optional<int16_t> milli_gramms;
};


