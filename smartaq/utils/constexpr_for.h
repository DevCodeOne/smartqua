#pragma once

template<size_t Index>
struct ConstexprFor final {
        template<typename TupleType, typename Callable>
        static void doCall(TupleType &tuple, Callable &&call) {
            call(std::get<Index>(tuple));
            if constexpr (Index > 0) {
                ConstexprFor<Index - 1>::doCall(tuple, call);
            }
        }

        template<typename Callable>
        constexpr static void doCall(Callable &&call) {
            call(std::integral_constant<size_t, Index>{});
            if constexpr (Index > 0) {
                ConstexprFor<Index - 1>::doCall(call);
            }
        }
};