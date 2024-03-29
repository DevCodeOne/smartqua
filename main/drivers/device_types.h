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

enum struct DeviceValueUnit {
    percentage = 0,
    temperature,
    ph,
    humidity,
    voltage,
    ampere,
    watt,
    tds,
    generic_analog,
    generic_pwm,
    milligramms,
    milliliter
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
    std::optional<uint16_t> tds;
    std::optional<uint16_t> generic_analog;
    std::optional<uint16_t> generic_pwm;
    std::optional<uint16_t> milligramms;
    std::optional<float> milliliter;
    /* Used to set possible values regardless of their unit e.g. set dac oder pwm signal from 0 - 100 % */
    std::optional<uint8_t> percentage;


    template<typename ValueType> 
    static device_values create_from_unit(DeviceValueUnit unit, ValueType value);
};

template<typename ValueType> 
device_values device_values::create_from_unit(DeviceValueUnit unit, ValueType value) {
    switch (unit) {
        case DeviceValueUnit::temperature:
            using TempType = decltype(std::declval<device_values>().temperature);
            return device_values { .temperature = static_cast<TempType>(value) };
        case DeviceValueUnit::ph:
            using PhType = decltype(std::declval<device_values>().ph);
            return device_values { .ph = static_cast<PhType>(value) };
        case DeviceValueUnit::humidity:
            using HumidType = decltype(std::declval<device_values>().humidity);
            return device_values { .humidity = static_cast<HumidType>(value) };
        case DeviceValueUnit::voltage:
            using VoltType = decltype(std::declval<device_values>().voltage);
            return device_values { .voltage = static_cast<VoltType>(value) };
        case DeviceValueUnit::ampere:
            using AmpType = decltype(std::declval<device_values>().ampere);
            return device_values { .ampere = static_cast<AmpType>(value) };
        case DeviceValueUnit::watt:
            using WattType = decltype(std::declval<device_values>().watt);
            return device_values { .watt = static_cast<WattType>(value) };
        case DeviceValueUnit::tds:
            using TdsType = decltype(std::declval<device_values>().tds);
            return device_values { .tds = static_cast<TdsType>(value) };
        case DeviceValueUnit::generic_analog:
            using GenericAnalogType = decltype(std::declval<device_values>().generic_analog);
            return device_values { .generic_analog = static_cast<GenericAnalogType>(value) };
        case DeviceValueUnit::milligramms:
            using MilliGrammsType = decltype(std::declval<device_values>().milligramms);
            return device_values { .milligramms = static_cast<MilliGrammsType>(value) };
        case DeviceValueUnit::milliliter:
            using MilliLiterType = decltype(std::declval<device_values>().milliliter);
            return device_values { .milliliter = static_cast<MilliLiterType>(value) };
        case DeviceValueUnit::percentage:
        default:
            using PercentageType = decltype(std::declval<device_values>().percentage);
            return device_values{ .percentage = static_cast<PercentageType>(value) };
    }
}

template<>
struct read_from_json<DeviceValueUnit> {
    static void read(const char *str, int len, DeviceValueUnit &unit) {
        std::string_view input(str, len);

        if (input == "percentage" || input == "%") {
            unit = DeviceValueUnit::percentage;
        } else if (input == "degc") {
            unit = DeviceValueUnit::temperature;
        } else if (input == "ml" || input == "milliliter") {
            unit = DeviceValueUnit::milliliter;
        } else if (input == "mg" || input == "milligramms") {
            unit = DeviceValueUnit::milligramms;
        } else if (input == "ph") {
            unit = DeviceValueUnit::ph;
        } else if (input == "humidity") {
            unit = DeviceValueUnit::humidity;
        } else if (input == "volt" || input == "v") {
            unit = DeviceValueUnit::voltage;
        } else if (input == "amp" || input == "a") {
            unit = DeviceValueUnit::ampere;
        } else if (input == "tds") {
            unit = DeviceValueUnit::tds;
        } else if (input == "analog") {
            unit = DeviceValueUnit::generic_analog;
        }
    }
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

        read_and_write_to_optional(str, len, "{ degrees : %f } ", values.temperature);
        read_and_write_to_optional(str, len, "{ ph : %f } ", values.ph);
        read_and_write_to_optional(str, len, "{ humidity : %f } ", values.humidity);
        read_and_write_to_optional(str, len, "{ voltage : %f } ", values.voltage);
        read_and_write_to_optional(str, len, "{ v : %f } ", values.voltage);
        read_and_write_to_optional(str, len, "{ ampere : %f } ", values.ampere);
        read_and_write_to_optional(str, len, "{ a : %f } ", values.ampere);
        read_and_write_to_optional(str, len, "{ watt : %f } ", values.watt);
        read_and_write_to_optional(str, len, "{ tds : %hd } ", values.tds);
        read_and_write_to_optional(str, len, "{ generic_analog : %hd } ", values.generic_analog);
        read_and_write_to_optional(str, len, "{ analog : %hd } ", values.generic_analog);
        read_and_write_to_optional(str, len, "{ generic_pwm : %hd } ", values.generic_pwm);
        read_and_write_to_optional(str, len, "{ pwm : %hd } ", values.generic_pwm);
        read_and_write_to_optional(str, len, "{ mg : %hd } ", values.milligramms);
        read_and_write_to_optional(str, len, "{ milliliter : %hd } ", values.milliliter);
        read_and_write_to_optional(str, len, "{ ml : %hd } ", values.milliliter);
        read_and_write_to_optional(str, len, "{ percentage : %hd } ", values.percentage);
    }
};
template<>
struct print_to_json<device_values> {
    static int print(json_out *out, const device_values &values) {
        int written = json_printf(out, "{");

        bool has_prev = false;

        auto write_optional = [&written, &has_prev](json_out *out, const char *fmt, auto &optional) {
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
        write_optional(out, ", percentage : %d", values.percentage);

        written += json_printf(out, "}");
        return written;
    }
};

