#pragma once

#include <type_traits>
#include <tuple>

#include "utils/utils.h"

template<typename T>
concept EnumType = std::is_enum_v<T>;

template<auto v, typename T>
requires (EnumType<decltype(v)>)
struct EnumTypePair {
    using type = T;
    static constexpr auto value = v;
};

template<typename ... ValueTypePairs>
requires (AllUniqueV<std::integral_constant<decltype(ValueTypePairs::value), ValueTypePairs::value> ...>)
struct EnumTypeMap {
    using KeyList = UniqueTypeList<std::integral_constant<std::decay_t<decltype(ValueTypePairs::value)>, ValueTypePairs::value> ...>;
    using KeyValueList = UniqueTypeList<ValueTypePairs ...>;

    template<auto EnumValue>
    static constexpr auto IndexOfEnumValue = KeyList::template IndexOf<std::integral_constant<std::decay_t<decltype(EnumValue)>, EnumValue>>;

    template<auto EnumValue>
    using map = KeyValueList::template TypeAt<IndexOfEnumValue<EnumValue>>::type;

};