#pragma once

namespace Detail
{
    template<typename Callable>
    concept ReturnsNoValue = std::is_void_v<std::invoke_result_t<Callable, std::integral_constant<size_t, 0>>>;

    template<typename Callable>
    concept ReturnsValue = !ReturnsNoValue<Callable>;
}

template <typename TupleType, typename Callable>
constexpr static void constexprFor(TupleType&& tuple, Callable&& call)
{
    std::apply([&call]<typename... T>(T&&... args)
    {
        (call(std::forward<T>(args)), ...);
    }, tuple);
}

template <size_t Index, Detail::ReturnsNoValue Callable>
constexpr static void constexprFor(Callable&& call)
{
    [&]<typename T, T ...Indices>(std::integer_sequence<T, Indices...>) {
        (..., call(std::integral_constant<size_t, Indices>{}));
    }(std::make_index_sequence<Index>{});
}

template <size_t MaxIndex, Detail::ReturnsNoValue Callable>
constexpr void callWithIndex(size_t Index, Callable&& callable)
{
    constexprFor<MaxIndex>(
        [callable, Index]<typename T, auto CurrentIndex>(const std::integral_constant<T, CurrentIndex>& index)
        {
            if (CurrentIndex == Index)
            {
                callable(index);
            }
        });
}
