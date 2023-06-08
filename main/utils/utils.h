#pragma once

#include <cstdint>
#include <tuple>
#include <type_traits>
#include <limits>
#include <variant>

template<typename Callable>
class DoFinally {
    public:
        DoFinally(Callable callable) : mCallable(callable) {}
        ~DoFinally() { mCallable(); }

    private:
        Callable mCallable;
};

template<typename Callable>
DoFinally(Callable callable) -> DoFinally<Callable>;

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

template<size_t Index>
struct constexpr_for_index {
    template<typename Callable>
    constexpr static void doCall(Callable &&call) {
        call(std::integral_constant<size_t, Index>{});
        constexpr_for_index<Index - 1>::doCall(call);
    }
};

template<>
struct constexpr_for_index<0> {
    template<typename Callable>
    constexpr static void doCall(Callable &&call) {
        call(std::integral_constant<size_t, 0>{});
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

template<typename T, typename Head, typename ... Tail>
struct FindTypeInList {
    static inline constexpr auto Index = std::is_same_v<T, Head> ? 0 : 1 + FindTypeInList<T, Tail ...>::Index;
};

template<typename T, typename Head>
struct FindTypeInList<T, Head> {
    static inline constexpr auto Index = std::is_same_v<T, Head> ? 0 : 1;
};


template<typename T, typename ... Types>
struct FindTypeInList<T, std::variant<Types ...>> {
    static inline constexpr auto Index = FindTypeInList<T, Types ...>::Index;
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

template<typename T, bool is_enum = std::is_enum_v<T>>
struct find_size_type {
    using type = T;
};

template<typename T>
struct find_size_type<T, true> {
    using type = std::underlying_type_t<T>;
};

template<typename T>
using find_size_type_t = typename find_size_type<T>::type;

template<typename T, typename T2>
bool check_assign(T &to_assign_to, T2 to_assign) {
    using size_type = find_size_type_t<T>;
    if (to_assign > std::numeric_limits<size_type>::max() || to_assign < std::numeric_limits<size_type>::min()) {
        return false;
    }

    to_assign_to = static_cast<T>(to_assign);
    return true;
}

