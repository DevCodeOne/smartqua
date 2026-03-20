#pragma once

#include <type_traits>

#include "utils/type/enum_type_map.h"
#include "utils/type/type_list.h"

template<typename EnumType, typename TypeMape>
class DeviceState;

template<typename EnumType, typename ... TypePairs>
requires (std::is_enum_v<EnumType>)
class DeviceState<EnumType, EnumTypeMap<TypePairs...>>
{
public:
    using EnumMap = EnumTypeMap<TypePairs...>;

    template<EnumType ... EnumValues>
    static constexpr auto makeValueTuple(EnumSequence<EnumType, EnumValues ...>)
    {
        return std::tuple<typename EnumMap::template map<EnumValues> ...>{};
    }

    using EnumSequence = EnumMap::KeyListSequence;
    using TupleType = decltype( makeValueTuple( EnumSequence{} ) );

    TupleType values;

    template<EnumType EnumValue>
    auto getValue() const
    {
        return std::get<EnumMap::template IndexOfEnumValue<EnumValue>>(values);
    }

    template<EnumType EnumValue>
    auto setValue(EnumMap::template map<EnumValue> newValue)
    {
        std::get<EnumMap::template IndexOfEnumValue<EnumValue>>(values) = newValue;
    }
};

template<typename T, typename = void>
struct HasDeviceState
{
    static constexpr bool value = false;
};

template<typename T>
struct HasDeviceState<T, std::void_t<typename T::State>>
{
    static constexpr bool value = IsInstantiationOf<DeviceState, typename T::State>::value;
};
