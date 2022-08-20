#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "smartqua_config.h"
#include "driver/gpio.h"
#include "utils/json_utils.h"

enum struct DeviceOperationResult {
    ok, not_supported, failure
};

struct device_config { 
    static inline constexpr char StorageName[] = "DeviceConfig";

    stack_string<name_length> device_driver_name;
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
    std::optional<int16_t> milligramms;
    /* Used to set possible values regardless of their unit e.g. set dac oder pwm signal from 0 - 100 % */
    std::optional<uint8_t> percentage;
};

template<>
struct read_from_json<device_values> {
    static void read(const char *str, int len, device_values &values) {
        auto read_and_write_to_optional = [](const char *input, int input_len, const char *format, auto &opt_value) {
            typename std::decay_t<decltype(opt_value)>::value_type read_value;
            int result = json_scanf(input, input_len, format, &read_value);
            if (result > 0) {
                opt_value = read_value;
                Logger::log(LogLevel::Info, "Writing to %s", format);
            }
        };

        read_and_write_to_optional(str, len, "{ temperature : %f } ", values.temperature);
        read_and_write_to_optional(str, len, "{ ph : %f } ", values.ph);
        read_and_write_to_optional(str, len, "{ humidity : %f } ", values.humidity);
        read_and_write_to_optional(str, len, "{ voltage : %f } ", values.voltage);
        read_and_write_to_optional(str, len, "{ ampere : %f } ", values.ampere);
        read_and_write_to_optional(str, len, "{ watt : %f } ", values.watt);
        read_and_write_to_optional(str, len, "{ tds : %hd } ", values.tds);
        read_and_write_to_optional(str, len, "{ generic_analog : %hd } ", values.generic_analog);
        read_and_write_to_optional(str, len, "{ generic_pwm : %hd } ", values.generic_pwm);
        read_and_write_to_optional(str, len, "{ milligramms : %hd } ", values.milligramms);
        read_and_write_to_optional(str, len, "{ percentage : %hd } ", values.percentage);
    }
};
template<>
struct print_to_json<device_values> {
    static int print(json_out *out, const device_values &values) {
        int written = json_printf(out, "{");

        bool has_prev = false;

        auto write_optional = [&out, &written, &has_prev](json_out *out, const char *fmt, auto &optional) {
            if (!optional) {
                return;
            }

            int newly_written = json_printf(out, fmt + (!has_prev ? 1 : 0), *optional);
            if (newly_written > 0) {
                has_prev = true;
            }
            written += newly_written;
        };

        write_optional(out, ", temperature : %f", values.temperature);
        write_optional(out, ", ph : %f", values.ph);
        write_optional(out, ", humidity : %f", values.humidity);
        write_optional(out, ", voltage : %f", values.voltage);
        write_optional(out, ", ampere : %f", values.ampere);
        write_optional(out, ", watt : %f", values.watt);
        write_optional(out, ", tds : %d", values.tds);
        write_optional(out, ", generic_analog : %d", values.generic_analog);
        write_optional(out, ", generic_pwm : %d", values.generic_pwm);
        write_optional(out, ", milligramms : %d", values.milligramms);
        write_optional(out, ", milligramms : %d", values.percentage);

        written += json_printf(out, "}");
        return written;
    }
};

