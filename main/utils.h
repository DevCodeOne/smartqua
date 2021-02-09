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