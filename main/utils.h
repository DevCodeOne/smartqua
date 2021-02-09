#pragma once

#include <cstdint>

template<size_t Index>
struct constexpr_for {
    template<typename TupleType, typename Callable>
    static void doCall(TupleType &tuple, Callable &&call) {
        call(std::get<Index>(tuple));
        constexpr_for<Index - 1>::doCall(tuple, call);
    }
};

template<>
struct constexpr_for<0> {
    template<typename TupleType, typename Callable>
    static void doCall(TupleType &tuple, Callable &&call) {
        call(std::get<0>(tuple));
    }
};

template<typename T, typename Head, typename ... Tail>
struct count_type {
    static inline constexpr auto value = (std::is_same_v<T, Head> ? 1 : 0) + count_type<T, Tail ...>::value;
};

template<typename T, typename Head>
struct count_type<T, Head> {
    static inline constexpr auto value = std::is_same_v<T, Head> ? 1 : 0; 
};

template<typename T, typename ... Types>
static inline constexpr auto count_type_v = count_type<T, Types ...>::value;

template<typename Index, typename ... Types>
struct all_unique {
    using current_type = std::tuple_element_t<Index::value, std::tuple<Types ...>>;

    static inline constexpr bool value = (count_type_v<current_type, Types ...> == 1) && 
    all_unique<std::integral_constant<size_t, Index::value - 1>, Types ...>::value;
};

template<typename ... Types>
struct all_unique<std::integral_constant<size_t, 0>, Types ...> {
    using current_type = std::tuple_element_t<0, std::tuple<Types ...>>;

    static inline constexpr bool value = (count_type_v<current_type, Types ...> == 1);
};

template<typename ... Types>
static inline constexpr bool all_unique_v = all_unique<std::integral_constant<size_t, sizeof...(Types) - 1>, Types ...>::value;

