#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "smartqua_config.h"
#include "driver/gpio.h"
#include "utils/json_utils.h"

enum struct device_read_result {
    ok, not_supported, failure
};

enum struct device_write_result {
    ok, not_supported, failure
};

struct device_config { 
    std::array<char, name_length> device_driver_name;
    // TODO: add method to write driver conf
    mutable std::array<char, device_config_size> device_config; // Binary data
};

struct device_values {
    std::optional<float> temperature;
    std::optional<float> ph;
    std::optional<float> humidity;
    std::optional<float> voltage;
    std::optional<float> ampere;
    std::optional<float> watt;
    std::optional<int16_t> tds;
    std::optional<int16_t> generic_analog;
    std::optional<int16_t> generic_pwm;
    std::optional<int16_t> milli_gramms;
};

template<>
struct print_to_json<device_values> {
    static int print(json_out *out, device_values &values) {
        int written = json_printf(out, "{");

        bool has_prev = false;

        if (values.temperature) {
            const char *fmt = ", temperature : %d.%d";
            auto [prev_point, after_point] = fixed_decimal_rep(*values.temperature);
            written += json_printf(out, fmt + (!has_prev ? 1 : 0), prev_point, after_point);
        }

        if (values.ph) {
            const char *fmt = ", ph : %d.%d";
            auto [prev_point, after_point] = fixed_decimal_rep(*values.ph);
            written += json_printf(out, fmt + (!has_prev ? 1 : 0), prev_point, after_point);
        }

        if (values.humidity) {
            const char *fmt = ", humidity : %d.%d";
            auto [prev_point, after_point] = fixed_decimal_rep(*values.humidity);
            written += json_printf(out, fmt + (!has_prev ? 1 : 0), prev_point, after_point);
        }

        if (values.voltage) {
            const char *fmt = ", voltage : %d.%d";
            auto [prev_point, after_point] = fixed_decimal_rep(*values.voltage);
            written += json_printf(out, fmt + (!has_prev ? 1 : 0), prev_point, after_point);
        }

        if (values.ampere) {
            const char *fmt = ", ampere : %d.%d";
            auto [prev_point, after_point] = fixed_decimal_rep(*values.ampere);
            written += json_printf(out, fmt + (!has_prev ? 1 : 0), prev_point, after_point);
        }

        if (values.watt) {
            const char *fmt = ", watt : %d.%d";
            auto [prev_point, after_point] = fixed_decimal_rep(*values.watt);
            written += json_printf(out, fmt + (!has_prev ? 1 : 0), prev_point, after_point);
        }

        if (values.tds) {
            const char *fmt = ", tds : %d";
            written += json_printf(out, fmt + (!has_prev ? 1 : 0), values.tds);
        }

        if (values.generic_analog) {
            const char *fmt = ", generic_analog : %d";
            written += json_printf(out, fmt + (!has_prev ? 1 : 0), values.generic_analog);
        }

        if (values.generic_pwm) {
            const char *fmt = ", generic_pwm : %d";
            written += json_printf(out, fmt + (!has_prev ? 1 : 0), values.generic_pwm);
        }

        if (values.milli_gramms) {
            const char *fmt = ", milligramms : %d";
            written += json_printf(out, fmt + (!has_prev ? 1 : 0), values.milli_gramms);
        }

        written += json_printf(out, "}");
        return written;
    }
};

