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
struct ConstexprFor {
    template<typename TupleType, typename Callable>
    static void doCall(TupleType &tuple, Callable &&call) {
        call(std::get<Index>(tuple));
        ConstexprFor<Index - 1>::doCall(tuple, call);
    }
};

template<>
struct ConstexprFor<0> {
    template<typename TupleType, typename Callable>
    static void doCall(TupleType &tuple, Callable &&call) {
        call(std::get<0>(tuple));
    }
};

template<size_t Index>
struct ConstexprForIndex {
    template<typename Callable>
    constexpr static void doCall(Callable &&call) {
        call(std::integral_constant<size_t, Index>{});
        ConstexprForIndex<Index - 1>::doCall(call);
    }
};

template<>
struct ConstexprForIndex<0> {
    template<typename Callable>
    constexpr static void doCall(Callable &&call) {
        call(std::integral_constant<size_t, 0>{});
    }
};

template<typename T, typename Head, typename ... Tail>
struct CountType {
    static inline constexpr auto value = (std::is_same_v<T, Head> ? 1 : 0) + CountType<T, Tail ...>::value;
};

template<typename T, typename Head>
struct CountType<T, Head> {
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
static inline constexpr auto count_type_v = CountType<T, Types ...>::value;

template<typename Index, typename ... Types>
struct AllUnique {
    using current_type = std::tuple_element_t<Index::value, std::tuple<Types ...>>;

    static inline constexpr bool value = (count_type_v<current_type, Types ...> == 1) &&
                                         AllUnique<std::integral_constant<size_t, Index::value - 1>, Types ...>::value;
};

template<typename ... Types>
struct AllUnique<std::integral_constant<size_t, 0>, Types ...> {
    using current_type = std::tuple_element_t<0, std::tuple<Types ...>>;

    static inline constexpr bool value = (count_type_v<current_type, Types ...> == 1);
};

template<typename ... Types>
static inline constexpr bool AllUniqueV = AllUnique<std::integral_constant<size_t, sizeof...(Types) - 1>, Types ...>::value;

template<typename T, bool is_enum = std::is_enum_v<T>>
struct FindSizeType {
    using type = T;
};

template<typename T>
struct FindSizeType<T, true> {
    using type = std::underlying_type_t<T>;
};

template<typename T>
using FindSizeTypeT = typename FindSizeType<T>::type;

template<typename T, typename T2>
bool checkAssign(T &to_assign_to, T2 to_assign) {
    using size_type = FindSizeTypeT<T>;
    if (to_assign > std::numeric_limits<size_type>::max() || to_assign < std::numeric_limits<size_type>::min()) {
        return false;
    }

    to_assign_to = static_cast<T>(to_assign);
    return true;
}

template<typename ... Types>
requires (AllUniqueV<Types ...>)
struct UniqueTypeList {
    template<typename T>
    static constexpr auto IndexOf = FindTypeInList<T, Types ...>::Index;

    template<auto Index>
    using TypeAt = std::tuple_element_t<Index, std::tuple<Types ...>>;
};