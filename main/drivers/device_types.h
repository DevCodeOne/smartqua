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
    std::array<char, name_length> device_driver_name;
    // TODO: add method to write driver conf
    std::array<char, device_config_size> device_config; // Binary data
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


