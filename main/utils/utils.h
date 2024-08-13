#pragma once

#include <cstdint>
#include <tuple>
#include <type_traits>
#include <limits>
#include <variant>

template<typename T, typename Head, typename ... Tail>
struct CountType {
    static constexpr auto value = (std::is_same_v<T, Head> ? 1 : 0) + CountType<T, Tail ...>::value;
};

template<typename T, typename Head>
struct CountType<T, Head> {
    static constexpr auto value = std::is_same_v<T, Head> ? 1 : 0;
};

template<typename T, typename Head, typename ... Tail>
struct FindTypeInList {
    static constexpr auto Index = std::is_same_v<T, Head> ? 0 : 1 + FindTypeInList<T, Tail ...>::Index;
};

template<typename T, typename Head>
struct FindTypeInList<T, Head> {
    static constexpr auto Index = std::is_same_v<T, Head> ? 0 : 1;
};

template<typename T, typename ... Types>
struct FindTypeInList<T, std::variant<Types ...>> {
    static inline constexpr auto Index = FindTypeInList<T, Types ...>::Index;
};

template<typename T, typename ... Types>
static inline constexpr auto CountTypeV = CountType<T, Types ...>::value;

template<typename Index, typename ... Types>
struct AllUnique {
    using CurrentType = std::tuple_element_t<Index::value, std::tuple<Types ...>>;

    static inline constexpr bool Value = (CountTypeV<CurrentType, Types ...> == 1) &&
                                         AllUnique<std::integral_constant<size_t, Index::value - 1>, Types ...>::Value;
};

template<typename ... Types>
struct AllUnique<std::integral_constant<size_t, 0>, Types ...> {
    using CurrentType = std::tuple_element_t<0, std::tuple<Types ...>>;

    static inline constexpr bool Value = (CountTypeV<CurrentType, Types ...> == 1);
};

template<typename ... Types>
static inline constexpr bool AllUniqueV = AllUnique<std::integral_constant<size_t, sizeof...(Types) - 1>, Types ...>::Value;

template<typename ... Types>
requires (AllUniqueV<Types ...>)
struct UniqueTypeList {
    template<typename T>
    static constexpr auto IndexOf = FindTypeInList<T, Types ...>::Index;

    template<auto Index>
    using TypeAt = std::tuple_element_t<Index, std::tuple<Types ...>>;
};