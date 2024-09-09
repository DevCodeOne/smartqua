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
#include "utils/enum_type_map.h"

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
    generic_unsigned_integral,
    milligrams,
    milliliter,
    enable,
    none = 0xFF
};

struct DeviceConfig { 
    static constexpr char StorageName[] = "DeviceConfig";

    stack_string<name_length> device_driver_name;
    // TODO: add method to write driver conf
    mutable std::array<char, device_config_size> device_config; // Binary data
};

using DeviceValueUnitMap = EnumTypeMap<
        EnumTypePair<DeviceValueUnit::temperature, float>,
        EnumTypePair<DeviceValueUnit::ph, float>,
        EnumTypePair<DeviceValueUnit::humidity, float>,
        EnumTypePair<DeviceValueUnit::voltage, float>,
        EnumTypePair<DeviceValueUnit::ampere, float>,
        EnumTypePair<DeviceValueUnit::watt, float>,
        EnumTypePair<DeviceValueUnit::tds, uint16_t>,
        EnumTypePair<DeviceValueUnit::generic_analog, uint16_t>,
        EnumTypePair<DeviceValueUnit::generic_pwm, uint16_t>,
        EnumTypePair<DeviceValueUnit::milligrams, int16_t>,
        EnumTypePair<DeviceValueUnit::milliliter, float>,
        EnumTypePair<DeviceValueUnit::enable, bool>,
        EnumTypePair<DeviceValueUnit::percentage, uint8_t>,
        EnumTypePair<DeviceValueUnit::generic_unsigned_integral, uint16_t>
        >;

template<DeviceValueUnit U>
struct UnitValue {
    static auto inline constexpr Unit = U;
    using UnderlyingDataType = typename DeviceValueUnitMap::map<Unit>;

    UnderlyingDataType mValue;

    constexpr UnitValue() = default;
    explicit constexpr UnitValue(UnderlyingDataType value) : mValue(value) {}

    template<typename T>
    explicit constexpr UnitValue(T value) : mValue(static_cast<UnderlyingDataType>(value)) {}

    template<typename T>
    UnitValue &operator=(const T &value) {
        mValue = static_cast<UnderlyingDataType>(value);
        return *this;
    }

};

// TODO: compare
// TODO: somehow variant is the correct type, but without dynamic access it's not useful, find a way, maybe only as runtime data
class DeviceValues {
    public:

    // TODO: return DeviceValues with unit
    template<typename T>
    std::optional<T> difference(const DeviceValues &other) const {
        // Not compatible
        if (index != other.index) {
            return { std::nullopt };
        }

        return { *getAsUnit<T>() - *other.getAsUnit<T>() };
    }

    template<typename T>
    std::optional<T> sum(const DeviceValues &other) const {
        // Not compatible
        if (index != other.index) {
            return { };
        }

        return { *getAsUnit<T>() + *other.getAsUnit<T>() };
    }

    template<typename T>
    [[nodiscard]] constexpr std::optional<T> getAsUnit(DeviceValueUnit unit) const {
        if (index != unit) {
            return std::nullopt;
        }

        switch (unit) {
            case DeviceValueUnit::temperature:
                return values.temperature.mValue;
            case DeviceValueUnit::ph:
                return values.ph.mValue;
            case DeviceValueUnit::humidity:
                return values.humidity.mValue;
            case DeviceValueUnit::voltage:
                return values.voltage.mValue;
            case DeviceValueUnit::ampere:
                return values.ampere.mValue;
            case DeviceValueUnit::watt:
                return values.watt.mValue;
            case DeviceValueUnit::tds:
                return values.tds.mValue;
            case DeviceValueUnit::generic_analog:
                return values.generic_analog.mValue;
            case DeviceValueUnit::generic_pwm:
                return values.generic_pwm.mValue;
            case DeviceValueUnit::generic_unsigned_integral:
                return values.un_integral.mValue;
            case DeviceValueUnit::milligrams:
                return values.milligrams.mValue;
            case DeviceValueUnit::milliliter:
                return values.milliliter.mValue;
            case DeviceValueUnit::enable:
                return values.enable.mValue;
            case DeviceValueUnit::percentage:
                return values.percentage.mValue;
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
    [[nodiscard]] constexpr auto getAsUnit() const {
        return getAsUnit<typename UnitValue<unit>::UnderlyingDataType>(unit);
    }

    template<DeviceValueUnit unit, typename T>
    void setToUnit(T value) {
        index = unit;
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
            case DeviceValueUnit::generic_unsigned_integral:
                values.un_integral = value;
                break;
            case DeviceValueUnit::milligrams:
                values.milligrams = value;
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

    [[nodiscard]] auto temperature() const { return getAsUnit<DeviceValueUnit::temperature>(); }

    [[nodiscard]] auto ph() const { return getAsUnit<DeviceValueUnit::ph>(); }

    [[nodiscard]] auto humidity() const { return getAsUnit<DeviceValueUnit::humidity>(); }

    [[nodiscard]] auto voltage() const { return getAsUnit<DeviceValueUnit::voltage>(); }

    [[nodiscard]] auto ampere() const { return getAsUnit<DeviceValueUnit::ampere>(); }

    [[nodiscard]] auto watt() const { return getAsUnit<DeviceValueUnit::watt>(); }

    [[nodiscard]] auto tds() const { return getAsUnit<DeviceValueUnit::tds>(); }

    [[nodiscard]] auto generic_analog() const { return getAsUnit<DeviceValueUnit::generic_analog>(); }

    [[nodiscard]] auto generic_pwm() const { return getAsUnit<DeviceValueUnit::generic_pwm>(); }

    [[nodiscard]] auto generic_unsigned_integral() const { return getAsUnit<DeviceValueUnit::generic_unsigned_integral>(); }

    [[nodiscard]] auto milligrams() const { return getAsUnit<DeviceValueUnit::milligrams>(); }

    [[nodiscard]] auto milliliter() const { return getAsUnit<DeviceValueUnit::milliliter>(); }

    [[nodiscard]] auto enable() const { return getAsUnit<DeviceValueUnit::enable>(); }

    [[nodiscard]] auto percentage() const { return getAsUnit<DeviceValueUnit::percentage>(); }

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
    void generic_unsigned_integral(T value) { return setToUnit<DeviceValueUnit::generic_unsigned_integral>(value); }

    template<typename T>
    void milligrams(T value) { return setToUnit<DeviceValueUnit::milligrams>(value); }

    template<typename T>
    void milliliter(T value) { return setToUnit<DeviceValueUnit::milliliter>(value); }

    template<typename T>
    void enable(T value) { return setToUnit<DeviceValueUnit::enable>(value); }

    template<typename T>
    void percentage(T value) { return setToUnit<DeviceValueUnit::percentage>(value); }

    template<typename ValueType> 
    static DeviceValues create_from_unit(DeviceValueUnit unit, ValueType value);

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
        UnitValue<DeviceValueUnit::milligrams> milligrams;
        UnitValue<DeviceValueUnit::milliliter> milliliter;
        UnitValue<DeviceValueUnit::enable> enable;
        UnitValue<DeviceValueUnit::percentage> percentage;
        UnitValue<DeviceValueUnit::generic_unsigned_integral> un_integral;
    } values;

    DeviceValueUnit index;

    static_assert(std::is_trivially_constructible_v<UnitValue<DeviceValueUnit::temperature>>, "Issue with variant types");
    static_assert(std::is_trivially_constructible_v<decltype(values)>, "Issue with variant");

    // TODO: use access methods later on
    friend struct read_from_json<DeviceValues>;
    friend struct print_to_json<DeviceValues>;

};


template<typename ValueType> 
DeviceValues DeviceValues::create_from_unit(DeviceValueUnit unit, ValueType value) {
    DeviceValues createdObject{};
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
        case DeviceValueUnit::generic_unsigned_integral:
            createdObject.values.un_integral = value;
            break;
        case DeviceValueUnit::milligrams:
            createdObject.values.milligrams = value;
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
        } else if (input == "mg" || input == "milligrams") {
            unit = DeviceValueUnit::milligrams;
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
        } else if (input == "un_integral") {
            unit = DeviceValueUnit::generic_unsigned_integral;
        } else if (input == "pwm") {
            unit = DeviceValueUnit::generic_pwm;
        }
    }
};

// TODO: map the units to strings
template<>
struct read_from_json<DeviceValues> {
    static void read(const char *str, int len, DeviceValues &values) {
        auto read_and_write_to_optional = []<typename UnitValueType>(const char *input,
                int input_len,
                const char *format,
                UnitValueType &unit_value,
                DeviceValueUnit &index) {
            typename UnitValueType::UnderlyingDataType read_value;
            int result = json_scanf(input, input_len, format, &read_value);
            if (result > 0) {
                unit_value = read_value;
                index = unit_value.Unit;
                Logger::log(LogLevel::Info, "Writing to %s", format);
            }
        };

        read_and_write_to_optional(str, len, "{ temperature : %f } ", values.values.temperature, values.index);
        read_and_write_to_optional(str, len, "{ ph : %f } ", values.values.ph, values.index);
        read_and_write_to_optional(str, len, "{ humidity : %f } ", values.values.humidity, values.index);
        read_and_write_to_optional(str, len, "{ voltage : %f } ", values.values.voltage, values.index);
        read_and_write_to_optional(str, len, "{ v : %f } ", values.values.voltage, values.index);
        read_and_write_to_optional(str, len, "{ ampere : %f } ", values.values.ampere, values.index);
        read_and_write_to_optional(str, len, "{ a : %f } ", values.values.ampere, values.index);
        read_and_write_to_optional(str, len, "{ watt : %f } ", values.values.watt, values.index);
        read_and_write_to_optional(str, len, "{ tds : %hd } ", values.values.tds, values.index);
        read_and_write_to_optional(str, len, "{ generic_analog : %hd } ", values.values.generic_analog, values.index);
        read_and_write_to_optional(str, len, "{ analog : %hd } ", values.values.generic_analog, values.index);
        read_and_write_to_optional(str, len, "{ generic_pwm : %hd } ", values.values.generic_pwm, values.index);
        read_and_write_to_optional(str, len, "{ pwm : %hd } ", values.values.generic_pwm, values.index);
        read_and_write_to_optional(str, len, "{ mg : %hd } ", values.values.milligrams, values.index);
        read_and_write_to_optional(str, len, "{ milliliter : %hd } ", values.values.milliliter, values.index);
        read_and_write_to_optional(str, len, "{ ml : %hd } ", values.values.milliliter, values.index);
        read_and_write_to_optional(str, len, "{ percentage : %hd } ", values.values.percentage, values.index);
        read_and_write_to_optional(str, len, "{ un_integral : %hd } ", values.values.un_integral, values.index);
    }
};

template<>
struct print_to_json<DeviceValues> {
    static int print(json_out *out, const DeviceValues &values) {
        int written = json_printf(out, "{");

        bool has_prev = false;

        auto write_optional = [&written, &has_prev]<typename UnitValue>(json_out *out, const char *fmt,
                                                                        DeviceValueUnit unit, UnitValue &unitValue) {
            if (UnitValue::Unit != unit) {
                return;
            }

            const int newly_written = json_printf(out, fmt + (!has_prev ? 1 : 0), unitValue.mValue);
            if (newly_written > 0) {
                has_prev = true;
            }
            written += newly_written;
        };

        write_optional(out, ", temperature : %f", values.index, values.values.temperature);
        write_optional(out, ", ph : %f", values.index, values.values.ph);
        write_optional(out, ", humidity : %f", values.index, values.values.humidity);
        write_optional(out, ", voltage : %f", values.index, values.values.voltage);
        write_optional(out, ", ampere : %f", values.index, values.values.ampere);
        write_optional(out, ", watt : %f", values.index, values.values.watt);
        write_optional(out, ", tds : %u", values.index, values.values.tds);
        write_optional(out, ", generic_analog : %u", values.index, values.values.generic_analog);
        write_optional(out, ", generic_pwm : %u", values.index, values.values.generic_pwm);
        write_optional(out, ", milliliter : %f", values.index, values.values.milliliter);
        write_optional(out, ", milligrams : %d", values.index, values.values.milligrams);
        write_optional(out, ", enable : %u", values.index, values.values.enable);
        write_optional(out, ", percentage : %u", values.index, values.values.percentage);

        written += json_printf(out, "}");
        return written;
    }
};

