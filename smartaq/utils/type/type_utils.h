#pragma once

#include <variant>

#include "utils/type/type_utils.h"

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

template <template <typename... > typename, typename>
struct IsInstantiationOf
{
    static constexpr bool value = false;
};

template <template <typename... > typename BaseTemplate, typename... Args>
struct IsInstantiationOf<BaseTemplate, BaseTemplate<Args...>>
{
    static constexpr bool value = true;
};
