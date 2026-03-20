#pragma once

#include "utils/type/type_list.h"
#include "utils/type/type_utils.h"

#include <cstddef>

template<typename Index, typename ... Types>
struct AllUnique {
    using CurrentType = std::tuple_element_t<Index::value, std::tuple<Types ...>>;

    static constexpr bool Value = (CountTypeV<CurrentType, Types ...> == 1) &&
                                         AllUnique<std::integral_constant<size_t, Index::value - 1>, Types ...>::Value;
};

template<typename ... Types>
struct AllUnique<std::integral_constant<size_t, 0>, Types ...> {
    using CurrentType = std::tuple_element_t<0, std::tuple<Types ...>>;

    static constexpr bool Value = (CountTypeV<CurrentType, Types ...> == 1);
};

template<typename ... Types>
static inline constexpr bool AllUniqueV = AllUnique<std::integral_constant<size_t, sizeof...(Types) - 1>, Types ...>::Value;

template<typename ... Types>
requires (AllUniqueV<Types ...>)
struct UniqueTypeList : TypeList<Types ...> {
    template<typename T>
    static constexpr auto IndexOf = FindTypeInList<T, Types ...>::Index;

    using AsTuple = std::tuple<Types ...>;

    template<auto Index>
    using TypeAt = std::tuple_element_t<Index, AsTuple>;

    static constexpr size_t Size = sizeof...(Types);
};
