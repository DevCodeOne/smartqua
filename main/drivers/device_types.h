#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <cmath>
#include <compare>
#include <type_traits>
#include <variant>

#include <frozen/unordered_map.h>
#include <frozen/string.h>

#include "build_config.h"
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
    milliliter,
    enable,
    none = 0xFF
};

struct DeviceConfig { 
    static inline constexpr char StorageName[] = "DeviceConfig";

    stack_string<name_length> device_driver_name;
    // TODO: add method to write driver conf
    mutable std::array<char, device_config_size> device_config; // Binary data
};

template<DeviceValueUnit unit>
struct UnitTypeMapping;

template<>
struct UnitTypeMapping<DeviceValueUnit::temperature> {
    using Type = float;
};

template<>
struct UnitTypeMapping<DeviceValueUnit::ph> {
    using Type = float;
};

template<>
struct UnitTypeMapping<DeviceValueUnit::humidity> {
    using Type = float;
};

template<>
struct UnitTypeMapping<DeviceValueUnit::voltage> {
    using Type = float;
};

template<>
struct UnitTypeMapping<DeviceValueUnit::ampere> {
    using Type = float;
};

template<>
struct UnitTypeMapping<DeviceValueUnit::watt> {
    using Type = float;
};

template<>
struct UnitTypeMapping<DeviceValueUnit::tds> {
    using Type = uint16_t;
};

template<>
struct UnitTypeMapping<DeviceValueUnit::generic_analog> {
    using Type = uint16_t;
};

template<>
struct UnitTypeMapping<DeviceValueUnit::generic_pwm> {
    using Type = uint16_t;
};

template<>
struct UnitTypeMapping<DeviceValueUnit::milligramms> {
    using Type = uint16_t;
};

template<>
struct UnitTypeMapping<DeviceValueUnit::milliliter> {
    using Type = float;
};

template<>
struct UnitTypeMapping<DeviceValueUnit::enable> {
    using Type = bool;
};

template<>
struct UnitTypeMapping<DeviceValueUnit::percentage> {
    using Type = bool;
};

template<DeviceValueUnit U>
struct UnitValue {
    static auto inline constexpr Unit = U;
    using UnderlyingDataType = typename UnitTypeMapping<Unit>::Type;

    UnderlyingDataType value;

    constexpr UnitValue() = default;
    constexpr UnitValue(UnderlyingDataType value) : value(value) {}

    template<typename T>
    constexpr UnitValue(T value) : value(static_cast<UnderlyingDataType>(value)) {}

};

// TODO: compare
// TODO: somehow variant is the correct type, but without dynamic access it's not useful, find a way, maybe only as runtime data
class device_values {
    public:

    // TODO: return device_values with unit
    template<typename T>
    std::optional<T> difference(device_values &other) const {
        // Not compatible
        if (index != other.index) {
            return std::nullopt;
        }

        return getAsUnit<T>() - other.getAsUnit<T>();
    }

    template<typename T>
    std::optional<T> sum(device_values &other) const {
        // Not compatible
        if (index != other.index) {
            return std::nullopt;
        }

        return getAsUnit<T>() + other.getAsUnit<T>();
    }

    template<typename T>
    constexpr std::optional<T> getAsUnit(DeviceValueUnit unit) const {
        if (index != unit) {
            return std::nullopt;
        }

        switch (unit) {
            case DeviceValueUnit::temperature:
                return values.temperature.value;
            case DeviceValueUnit::ph:
                return values.ph.value;
            case DeviceValueUnit::humidity:
                return values.humidity.value;
            case DeviceValueUnit::voltage:
                return values.voltage.value;
            case DeviceValueUnit::ampere:
                return values.ampere.value;
            case DeviceValueUnit::watt:
                return values.watt.value;
            case DeviceValueUnit::tds:
                return values.tds.value;
            case DeviceValueUnit::generic_analog:
                return values.generic_analog.value;
            case DeviceValueUnit::generic_pwm:
                return values.generic_pwm.value;
            case DeviceValueUnit::milligramms:
                return values.milligramms.value;
            case DeviceValueUnit::milliliter:
                return values.milliliter.value;
            case DeviceValueUnit::enable:
                return values.enable.value;
            case DeviceValueUnit::percentage:
                return values.percentage.value;
            case DeviceValueUnit::none:
            default:
                return std::nullopt;
        }

        return std::nullopt;
    }


    template<typename T>
    constexpr std::optional<T> getAsUnit() const {
        return getAsUnit<T>(index);
    }

    template<DeviceValueUnit unit>
    constexpr auto getAsUnit() const {
        return getAsUnit<typename UnitValue<unit>::UnderlyingDataType>(unit);
    }

    template<DeviceValueUnit unit, typename T>
    void setToUnit(T value) {
        switch (unit) {
            case DeviceValueUnit::temperature:
                values.temperature = value;
                break;
            case DeviceValueUnit::ph:
                values.ph = value;
                break;
            case DeviceValueUnit::humidity:
                values.humidity = value;
                break;
            case DeviceValueUnit::voltage:
                values.voltage = value;
                break;
            case DeviceValueUnit::ampere:
                values.ampere = value;
                break;
            case DeviceValueUnit::watt:
                values.watt = value;
                break;
            case DeviceValueUnit::tds:
                values.tds = value;
                break;
            case DeviceValueUnit::generic_analog:
                values.generic_analog = value;
                break;
            case DeviceValueUnit::generic_pwm:
                values.generic_pwm = value;
                break;
            case DeviceValueUnit::milligramms:
                values.milligramms = value;
                break;
            case DeviceValueUnit::milliliter:
                values.milliliter = value;
                break;
            case DeviceValueUnit::enable:
                values.enable = value;
                break;
            case DeviceValueUnit::percentage:
                values.percentage = value;
                break;
        }
    }

    auto temperature() const { return getAsUnit<DeviceValueUnit::temperature>(); }

    auto ph() const { return getAsUnit<DeviceValueUnit::ph>(); }

    auto humidity() const { return getAsUnit<DeviceValueUnit::humidity>(); }

    auto voltage() const { return getAsUnit<DeviceValueUnit::voltage>(); }

    auto ampere() const { return getAsUnit<DeviceValueUnit::ampere>(); }

    auto watt() const { return getAsUnit<DeviceValueUnit::watt>(); }

    auto tds() const { return getAsUnit<DeviceValueUnit::tds>(); }

    auto generic_analog() const { return getAsUnit<DeviceValueUnit::generic_analog>(); }

    auto generic_pwm() const { return getAsUnit<DeviceValueUnit::generic_pwm>(); }

    auto milligramms() const { return getAsUnit<DeviceValueUnit::milligramms>(); }

    auto milliliter() const { return getAsUnit<DeviceValueUnit::milliliter>(); }

    auto enable() const { return getAsUnit<DeviceValueUnit::enable>(); }

    auto percentage() const { return getAsUnit<DeviceValueUnit::percentage>(); }

    template<typename T>
    void temperature(T value) { return setToUnit<DeviceValueUnit::temperature>(value); }

    template<typename T>
    void ph(T value) { return setToUnit<DeviceValueUnit::ph>(value); }

    template<typename T>
    void humidity(T value) { return setToUnit<DeviceValueUnit::humidity>(value); }

    template<typename T>
    void voltage(T value) { return setToUnit<DeviceValueUnit::voltage>(value); }

    template<typename T>
    void ampere(T value) { return setToUnit<DeviceValueUnit::ampere>(value); }

    template<typename T>
    void watt(T value) { return setToUnit<DeviceValueUnit::watt>(value); }

    template<typename T>
    void tds(T value) { return setToUnit<DeviceValueUnit::tds>(value); }

    template<typename T>
    void generic_analog(T value) { return setToUnit<DeviceValueUnit::generic_analog>(value); }

    template<typename T>
    void generic_pwm(T value) { return setToUnit<DeviceValueUnit::generic_pwm>(value); }

    template<typename T>
    void milligramms(T value) { return setToUnit<DeviceValueUnit::milligramms>(value); }

    template<typename T>
    void milliliter(T value) { return setToUnit<DeviceValueUnit::milliliter>(value); }

    template<typename T>
    void enable(T value) { return setToUnit<DeviceValueUnit::enable>(value); }

    template<typename T>
    void percentage(T value) { return setToUnit<DeviceValueUnit::percentage>(value); }

    template<typename ValueType> 
    static device_values create_from_unit(DeviceValueUnit unit, ValueType value);

    private:

    union {
        UnitValue<DeviceValueUnit::temperature> temperature;
        UnitValue<DeviceValueUnit::ph> ph;
        UnitValue<DeviceValueUnit::humidity> humidity;
        UnitValue<DeviceValueUnit::voltage> voltage;
        UnitValue<DeviceValueUnit::ampere> ampere;
        UnitValue<DeviceValueUnit::watt> watt;
        UnitValue<DeviceValueUnit::tds> tds;
        UnitValue<DeviceValueUnit::generic_analog> generic_analog;
        UnitValue<DeviceValueUnit::generic_pwm> generic_pwm;
        UnitValue<DeviceValueUnit::milligramms> milligramms;
        UnitValue<DeviceValueUnit::milliliter> milliliter;
        UnitValue<DeviceValueUnit::enable> enable;
        UnitValue<DeviceValueUnit::percentage> percentage;
    } values;

    DeviceValueUnit index;

    static_assert(std::is_trivially_constructible_v<UnitValue<DeviceValueUnit::temperature>>, "Issue with variant types");
    static_assert(std::is_trivially_constructible_v<decltype(values)>, "Issue with variant");

    // TODO: use access methods later on
    friend struct read_from_json<device_values>;
    friend struct print_to_json<device_values>;

};


template<typename ValueType> 
device_values device_values::create_from_unit(DeviceValueUnit unit, ValueType value) {
    device_values createdObject{};
    createdObject.index = unit;
    switch (unit) {
        case DeviceValueUnit::temperature:
            createdObject.values.temperature = value;
            break;
        case DeviceValueUnit::ph:
            createdObject.values.ph = value;
            break;
        case DeviceValueUnit::humidity:
            createdObject.values.humidity = value;
            break;
        case DeviceValueUnit::voltage:
            createdObject.values.voltage = value;
            break;
        case DeviceValueUnit::ampere:
            createdObject.values.ampere = value;
            break;
        case DeviceValueUnit::watt:
            createdObject.values.watt = value;
            break;
        case DeviceValueUnit::tds:
            createdObject.values.tds = value;
            break;
        case DeviceValueUnit::generic_analog:
            createdObject.values.generic_analog = value;
            break;
        case DeviceValueUnit::milligramms:
            createdObject.values.milligramms = value;
            break;
        case DeviceValueUnit::milliliter:
            createdObject.values.milliliter = value;
            break;
        case DeviceValueUnit::enable:
            createdObject.values.enable = value;
            break;
        case DeviceValueUnit::percentage:
        default:
            createdObject.values.percentage = value;
            break;
    }

    return createdObject;
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
        } else if (input == "bool" || input == "switch" || input == "enable") {
            unit = DeviceValueUnit::enable;
        }
    }
};

template<>
struct read_from_json<device_values> {
    static void read(const char *str, int len, device_values &values) {
        auto read_and_write_to_optional = [](const char *input, int input_len, const char *format, auto &unit_value) {
            typename std::decay_t<decltype(unit_value)>::UnderlyingDataType read_value;
            int result = json_scanf(input, input_len, format, &read_value);
            if (result > 0) {
                unit_value = read_value;
                Logger::log(LogLevel::Info, "Writing to %s", format);
            }
        };

        read_and_write_to_optional(str, len, "{ degrees : %f } ", values.values.temperature);
        read_and_write_to_optional(str, len, "{ ph : %f } ", values.values.ph);
        read_and_write_to_optional(str, len, "{ humidity : %f } ", values.values.humidity);
        read_and_write_to_optional(str, len, "{ voltage : %f } ", values.values.voltage);
        read_and_write_to_optional(str, len, "{ v : %f } ", values.values.voltage);
        read_and_write_to_optional(str, len, "{ ampere : %f } ", values.values.ampere);
        read_and_write_to_optional(str, len, "{ a : %f } ", values.values.ampere);
        read_and_write_to_optional(str, len, "{ watt : %f } ", values.values.watt);
        read_and_write_to_optional(str, len, "{ tds : %hd } ", values.values.tds);
        read_and_write_to_optional(str, len, "{ generic_analog : %hd } ", values.values.generic_analog);
        read_and_write_to_optional(str, len, "{ analog : %hd } ", values.values.generic_analog);
        read_and_write_to_optional(str, len, "{ generic_pwm : %hd } ", values.values.generic_pwm);
        read_and_write_to_optional(str, len, "{ pwm : %hd } ", values.values.generic_pwm);
        read_and_write_to_optional(str, len, "{ mg : %hd } ", values.values.milligramms);
        read_and_write_to_optional(str, len, "{ milliliter : %hd } ", values.values.milliliter);
        read_and_write_to_optional(str, len, "{ ml : %hd } ", values.values.milliliter);
        read_and_write_to_optional(str, len, "{ percentage : %hd } ", values.values.percentage);
    }
};

template<>
struct print_to_json<device_values> {
    static int print(json_out *out, const device_values &values) {
        int written = json_printf(out, "{");

        bool has_prev = false;

        auto write_optional = [&written, &has_prev](json_out *out, const char *fmt, DeviceValueUnit unit, auto &unitValue) {
            if (std::decay_t<decltype(unitValue)>::Unit != unit) {
                return;
            }

            int newly_written = json_printf(out, fmt + (!has_prev ? 1 : 0), unitValue.value);
            if (newly_written > 0) {
                has_prev = true;
            }
            written += newly_written;
        };

        write_optional(out, ", temperature : %f", DeviceValueUnit::temperature, values.values.temperature);
        write_optional(out, ", ph : %f", DeviceValueUnit::ph, values.values.ph);
        write_optional(out, ", humidity : %f", DeviceValueUnit::humidity, values.values.humidity);
        write_optional(out, ", voltage : %f", DeviceValueUnit::voltage, values.values.voltage);
        write_optional(out, ", ampere : %f", DeviceValueUnit::ampere, values.values.ampere);
        write_optional(out, ", watt : %f", DeviceValueUnit::watt, values.values.watt);
        write_optional(out, ", tds : %u", DeviceValueUnit::tds, values.values.tds);
        write_optional(out, ", generic_analog : %u", DeviceValueUnit::generic_analog, values.values.generic_analog);
        write_optional(out, ", generic_pwm : %u", DeviceValueUnit::generic_pwm, values.values.generic_pwm);
        write_optional(out, ", milliliter : %f", DeviceValueUnit::milliliter, values.values.milliliter);
        write_optional(out, ", milligramms : %d", DeviceValueUnit::milligramms, values.values.milligramms);
        write_optional(out, ", enable : %u", DeviceValueUnit::percentage, values.values.enable);
        write_optional(out, ", percentage : %u", DeviceValueUnit::percentage, values.values.percentage);

        written += json_printf(out, "}");
        return written;
    }
};

