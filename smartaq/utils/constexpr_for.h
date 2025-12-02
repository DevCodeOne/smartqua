#pragma once

namespace Detail
{
    template<typename Callable>
    concept ReturnsNoValue = std::is_void_v<std::invoke_result_t<Callable, std::integral_constant<size_t, 0>>>;

    template<typename Callable>
    concept ReturnsValue = !ReturnsNoValue<Callable>;
}

template<size_t Index>
struct ConstexprFor final
{
    template <typename TupleType, typename Callable>
    constexpr static void doCall(TupleType& tuple, Callable&& call)
    {
        call(std::get<Index>(tuple));
        if constexpr (Index > 0)
        {
            ConstexprFor<Index - 1>::doCall(tuple, call);
        }
    }

    template <typename TupleType, typename Callable>
    constexpr static void doCall(const TupleType& tuple, Callable&& call)
    {
        call(std::get<Index>(tuple));
        if constexpr (Index > 0)
        {
            ConstexprFor<Index - 1>::doCall(tuple, call);
        }
    }

    template <Detail::ReturnsNoValue Callable>
    constexpr static void doCall(Callable&& call)
    {
        call(std::integral_constant<size_t, Index>{});
        if constexpr (Index > 0)
        {
            ConstexprFor<Index - 1>::doCall(call);
        }
    }

};

template <size_t MaxIndex, Detail::ReturnsNoValue Callable>
constexpr void callWithIndex(size_t Index, Callable&& callable)
{
    ConstexprFor<MaxIndex>::doCall(
        [callable, Index]<typename T, auto CurrentIndex>(const std::integral_constant<T, CurrentIndex>& index)
        {
            if (CurrentIndex == Index)
            {
                callable(index);
            }
        });
}
