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

namespace Detail
{
    template<typename T, typename Head, typename ... Tail>
    struct FindTypeInList {
        static constexpr auto Index = std::is_same_v<T, Head> ? 0 : 1 + FindTypeInList<T, Tail ...>::Index;
    };

    template<typename T, typename Head>
    struct FindTypeInList<T, Head> {
        static constexpr auto Index = std::is_same_v<T, Head> ? 0 : 1;
    };
}

template<typename T, typename ... Types>
requires (std::is_same_v<T, Types> || ...)
struct FindTypeInList
{
    static constexpr auto Index = Detail::FindTypeInList<T, Types ...>::Index;
};

template<typename T, typename ... Types>
requires (std::is_same_v<T, Types> || ...)
struct FindTypeInList<T, std::variant<Types ...>> {
    static constexpr auto Index = FindTypeInList<T, Types ...>::Index;
};

template<typename T, typename ... Types>
requires (std::is_same_v<T, Types> || ...)
struct FindTypeInList<T, std::tuple<Types ...>> {
    static constexpr auto Index = FindTypeInList<T, Types ...>::Index;
};

template<typename T, typename ... Types>
static inline constexpr auto CountTypeV = CountType<T, Types ...>::value;

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
struct UniqueTypeList {
    template<typename T>
    static constexpr auto IndexOf = FindTypeInList<T, Types ...>::Index;

    using AsTuple = std::tuple<Types ...>;

    template<auto Index>
    using TypeAt = std::tuple_element_t<Index, AsTuple>;

    static constexpr size_t Size = sizeof...(Types);
};

template<auto Value, typename ... Types>
requires (std::is_same_v<std::common_type_t<Types ...>, decltype(Value)>)
bool allEqualTo(Types ... arguments)
{
    return ((Value == arguments) && ... );
}
