#pragma once

#include <frozen/unordered_map.h>
#include <frozen/string.h>
#include <frozen/set.h>

#include "type/type_helper.h"
#include "utils/constexpr_for.h"
#include "utils/type/enum_type_map.h"
#include "utils/serialization/json_utils.h"

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
    seconds,
    none = seconds + 1
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
        EnumTypePair<DeviceValueUnit::seconds, uint16_t>,
        EnumTypePair<DeviceValueUnit::generic_unsigned_integral, uint16_t>,
        EnumTypePair<DeviceValueUnit::none, bool>
        >;

using DeviceValueUnitList = DeviceValueUnitMap::KeyList;

template<DeviceValueUnit U>
struct UnitValue {
    static auto constexpr Unit = U;
    using UnderlyingDataType = DeviceValueUnitMap::map<Unit>;

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

template<typename T, template <typename ... OtherArgs> typename ToBind>
struct BindArg
{
    template<typename ... OtherArgs>
    using Type = ToBind<T, OtherArgs ...>;

    template<typename ... OtherArgs>
    static constexpr auto value = ToBind<T, OtherArgs ...>::value;
};

struct DeviceValueUnion {
    using Types = UniqueTypeList<float, uint8_t, uint16_t, int16_t, bool>;

    template<typename T>
    [[nodiscard]] constexpr std::optional<T> get() const {
        using IsType = BindArg<T, std::is_same>;
        if constexpr (IsType::template value<float>)
        {
            return {mValues.asFloatValue};
        }
        if constexpr (IsType::template value<uint16_t>)
        {
            return {mValues.asUnsigned};
        }
        if constexpr (IsType::template value<int16_t>)
        {
            return {mValues.asSigned};
        }
        if constexpr (IsType::template value<uint8_t>)
        {
            return {mValues.asUnsigned8};
        }
        if constexpr (IsType::template value<bool>)
        {
            return {mValues.asBool};
        }
        return {};
    }

    template <typename T>
    [[nodiscard]] constexpr bool set(T newValue)
    {
        using IsType = BindArg<T, std::is_same>;
        if constexpr (IsType::template value<float>)
        {
            mValues.asFloatValue = newValue;
        }
        else if constexpr (IsType::template value<uint16_t>)
        {
            mValues.asUnsigned = newValue;
        }
        else if constexpr (IsType::template value<int16_t>)
        {
            mValues.asSigned = newValue;
        }
        else if constexpr (IsType::template value<uint8_t>)
        {
            mValues.asUnsigned8 = newValue;
        }
        else if constexpr (IsType::template value<bool>)
        {
            mValues.asBool = newValue;
        }
        else
        {
            return false;
        }
        return true;
    }

    union RawUnion
    {
        float asFloatValue;
        uint16_t asUnsigned;
        int16_t asSigned;
        uint8_t asUnsigned8;
        bool asBool;
    } mValues;
};

// TODO: compare
// TODO: somehow variant is the correct type, but without dynamic access it's not useful, find a way, maybe only as runtime data
class DeviceValues {
    public:

    [[nodiscard]] std::optional<DeviceValues> difference(const DeviceValues &other) const;
    [[nodiscard]] std::optional<DeviceValues> sum(const DeviceValues &other) const;
    template<typename Callable>
    [[nodiscard]] std::optional<DeviceValues> operation(const DeviceValues &other, Callable operation) const;

    template<typename T, bool DoConvert = false>
    [[nodiscard]] constexpr std::optional<T> getAsUnit(DeviceValueUnit unit) const {
        if (index != unit || index == DeviceValueUnit::none) {
            return std::nullopt;
        }

        std::optional<T> foundValue{};

        auto isThisType = [this, &foundValue]<typename UnitType>(UnitType) {
            constexpr auto typeValues = DeviceValueUnitMap::collectKeysWithType<UnitType>();
            if (std::ranges::contains(typeValues, index))
            {
                if constexpr (std::is_same_v<UnitType, T> || DoConvert)
                {
                    // TODO: Check if cast is supported
                    auto returnedValue = values.get<UnitType>();

                    if (returnedValue.has_value())
                    {
                        foundValue = static_cast<T>(returnedValue.value());
                    }
                }
            }
        };

        ConstexprFor<DeviceValueUnion::Types::Size - 1>::doCall(DeviceValueUnion::Types::AsTuple{}, [&](auto CurrentUnit)
        {
            isThisType(CurrentUnit);
        });

        return foundValue;
    }

    void invalidate()
    {
        index = DeviceValueUnit::none;
    }

    template<typename T, bool DoConvert = false>
    constexpr std::optional<T> getAsUnit() const {
        return getAsUnit<T, DoConvert>(index);
    }

    template<DeviceValueUnit unit>
    [[nodiscard]] constexpr auto getAsUnitAsType() const {
        return getAsUnit<typename UnitValue<unit>::UnderlyingDataType>(unit);
    }

    template<DeviceValueUnit unit, typename T>
    void setToUnit(T value) {
        setToUnit(unit, value);
    }

    template<typename T>
    bool setToUnit(DeviceValueUnit unit, T value) {
        if (unit == DeviceValueUnit::none)
        {
            return false;
        }

        auto isThisType = [this, &value, unit]<typename UnitType>(UnitType) {
            constexpr auto typeValues = DeviceValueUnitMap::collectKeysWithType<UnitType>();
            if (std::ranges::contains(typeValues, unit))
            {
                // TODO: Do Potential cast
                index = unit;
                return values.set<UnitType>(value);
            }
            return false;
        };

        bool didSet = false;
        ConstexprFor<DeviceValueUnion::Types::Size - 1>::doCall(DeviceValueUnion::Types::AsTuple{}, [&](auto CurrentUnit)
        {
            didSet |= isThisType(CurrentUnit);
        });

        return didSet;
    }

    template<typename ValueType>
    static DeviceValues create_from_unit(DeviceValueUnit unit, ValueType value);
    static DeviceValues create_empty();

    [[nodiscard]] auto getUnit() const { return index; }

    bool operator==(const DeviceValues &other) const
    {
        if (index != other.index)
        {
            return false;
        }

        bool compareValues = false;

        callWithIndex<DeviceValueUnitMap::Size - 1>(std::to_underlying(index),
                                                    [&]<typename T, auto Index>(
                                                    const std::integral_constant<T, Index>&)
                                                    {
                                                        constexpr DeviceValueUnit currentValueUnit{Index};
                                                        compareValues = getAsUnitAsType<currentValueUnit>() == other.
                                                            getAsUnitAsType<
                                                                currentValueUnit>();
                                                    });
        return compareValues;
    }

    bool operator!=(const DeviceValues &other) const = default;

    private:

    DeviceValueUnion values;

    DeviceValueUnit index;

    static_assert(std::is_trivially_constructible_v<UnitValue<DeviceValueUnit::temperature>>, "Issue with variant types");
    static_assert(std::is_trivially_constructible_v<decltype(values)>, "Issue with variant");

    // TODO: use access methods later on
    friend struct read_from_json<DeviceValues>;
    friend struct print_to_json<DeviceValues>;

};

template <typename Callable>
std::optional<DeviceValues> DeviceValues::operation(const DeviceValues& other, Callable operation) const
{
    // Not compatible
    if (index != other.index || index == DeviceValueUnit::none || other.index == DeviceValueUnit::none) {
        return { };
    }

    std::optional<DeviceValues> result{};
    ConstexprFor<DeviceValueUnitList::Size - 1>::doCall([&]<size_t Index>(std::integral_constant<size_t, Index>)
    {
        constexpr auto CurrentUnit = DeviceValueUnitList::TypeAt<Index>::value;
        if (index == CurrentUnit)
        {
            const auto thisAsUnderlying = getAsUnitAsType<CurrentUnit>();
            const auto otherAsUnderlying = other.getAsUnitAsType<CurrentUnit>();

            if (thisAsUnderlying.has_value() && otherAsUnderlying.has_value())
            {
                result = create_from_unit(CurrentUnit, operation(thisAsUnderlying.value(), otherAsUnderlying.value()));
            }
        }
    });
    return result;
}

template<typename ValueType>
DeviceValues DeviceValues::create_from_unit(DeviceValueUnit unit, ValueType value) {
    DeviceValues createdObject{};
    if (unit == DeviceValueUnit::none)
    {
        createdObject.invalidate();
        return createdObject;
    }

    createdObject.setToUnit(unit, value);
    return createdObject;
}


inline DeviceValues DeviceValues::create_empty() {
    using NoneType = UnitValue<DeviceValueUnit::none>::UnderlyingDataType;
    return create_from_unit(DeviceValueUnit::none, NoneType{});
}

namespace Detail
{
    // 1) Your variant of frozen sets with sizes 1..Max
    template<std::size_t... Ns>
    using frozen_variant_impl = std::variant<std::array<frozen::string, Ns>...>;

    template<std::size_t Max, std::size_t... Is>
    auto make_frozen_variant_impl(std::index_sequence<Is...>)
      -> frozen_variant_impl<(Is + 1)...> { return {}; }

    template<std::size_t Max>
    using make_frozen_variant_t =
      decltype(make_frozen_variant_impl<Max>(std::make_index_sequence<Max>{}));

    // 2) Choose the max alias count you expect per unit
    constexpr std::size_t MaxAliasPerUnit = 5;
    using KeyType = make_frozen_variant_t<MaxAliasPerUnit >;

    // 3) Variadic helper: makeUnitValuePair(unit, "alias"_fstr, "alias2"_fstr, ...)
    template <typename... Ss>
    constexpr auto makeUnitValuePair(DeviceValueUnit u, Ss... ss)
      -> std::pair<DeviceValueUnit, KeyType>
    {
        // All arguments must be frozen::string (use "_fstr" literal)
        static_assert((std::is_same_v<Ss, frozen::string> && ...),
                      "Aliases must be frozen::string; use \"text\"_s.");
        static_assert(sizeof...(Ss) >= 1, "Provide at least one alias.");
        static_assert(sizeof...(Ss) <= MaxAliasPerUnit,
                      "Too many aliases for KeyType ceiling; bump kMaxAliasesPerUnit.");

        // Build a frozen set with the exact arity (N = sizeof...(Ss))
        auto setN = std::array<frozen::string, sizeof...(ss)>{ ss... };

        // Let the variant pick the correct alternative (set size) automatically
        return { u, KeyType{ setN } };
    }

    using namespace frozen::string_literals;
    static constexpr auto UnitNames = frozen::make_set<std::pair<DeviceValueUnit, KeyType>>({
                makeUnitValuePair(DeviceValueUnit::temperature, "temperature"_s, "degc"_s, "celsius"_s, "c"_s),
                makeUnitValuePair(DeviceValueUnit::ph, "ph"_s),
                makeUnitValuePair(DeviceValueUnit::humidity, "humidity"_s),
                makeUnitValuePair(DeviceValueUnit::voltage, "voltage"_s, "v"_s, "volt"_s),
                makeUnitValuePair(DeviceValueUnit::ampere, "ampere"_s, "a"_s, "amp"_s),
                makeUnitValuePair(DeviceValueUnit::watt, "watt"_s),
                makeUnitValuePair(DeviceValueUnit::tds, "tds"_s),
                makeUnitValuePair(DeviceValueUnit::generic_analog, "generic_analog"_s, "analog"_s),
                makeUnitValuePair(DeviceValueUnit::generic_pwm, "generic_pwm"_s),
                makeUnitValuePair(DeviceValueUnit::milligrams, "milligrams"_s, "mg"_s),
                makeUnitValuePair(DeviceValueUnit::milliliter, "milliliter"_s, "ml"_s),
                makeUnitValuePair(DeviceValueUnit::enable, "enable"_s, "bool"_s, "switch"_s),
                makeUnitValuePair(DeviceValueUnit::percentage, "percentage"_s, "%"_s),
                makeUnitValuePair(DeviceValueUnit::seconds, "seconds"_s, "s"_s, "sec"_s ),
                makeUnitValuePair(DeviceValueUnit::generic_unsigned_integral, "generic_unsigned_integral"_s),
                makeUnitValuePair(DeviceValueUnit::generic_pwm,   "pwm"_s)
            });
}

template<>
struct read_from_json<DeviceValueUnit> {
    static void read(const char *str, int len, DeviceValueUnit &unit) {
        std::string_view input(str, len);

        const auto matchingUnit = std::find_if(Detail::UnitNames.begin(), Detail::UnitNames.end(),
                                               [&input](
                                               const std::pair<DeviceValueUnit, Detail::KeyType>& currentEntry)
                                               {
                                                   return std::visit([&input](const auto& realList)
                                                   {
                                                       return std::ranges::contains(realList, input);
                                                   }, currentEntry.second);
                                               });

        if (matchingUnit == Detail::UnitNames.end())
        {
            unit = DeviceValueUnit::none;
            return;
        }

        unit = matchingUnit->first;
    }
};

template<bool Reading, auto Size, typename T>
[[nodiscard]] BasicStackString<Size> create_json_format(const std::string_view &key, const std::optional<T> &) {
    BasicStackString<Size> format{"{"};

    format.append(key);
    format.append(":");

    if constexpr (std::is_floating_point_v<T>)
    {
        // Floating-point type: Use %.2f format for two decimal places
        if (Reading)
        {
            format.append(R"(%f})");
        }
        else
        {
            format.append(R"(%.5f})");
        }
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        // Boolean: Use %d (0/1)
        format.append(R"(%B})");
    }
    else if constexpr (isArithmeticInteger<T>)
    {
        // Integer types: Use %d
        format.append(R"(%d})");
    }

    return format;
}
