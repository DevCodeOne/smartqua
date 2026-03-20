#pragma once

#include <tuple>
#include <cstddef>

template<typename ... Types>
struct TypeList : std::tuple<Types ...>
{
    template<typename T>
    using add = TypeList<T, Types ...>;

    template<typename T>
    static constexpr bool contains = (std::is_same_v<T, Types>  || ... );

    static constexpr size_t size = sizeof...(Types);

    template<template <typename ...> typename T>
    using generateType = T<Types ...>;

    template<size_t Index>
    using typeAt = std::tuple_element_t<Index, TypeList>;

    template<template <typename> typename T>
    using transform = TypeList<typename T<Types>::type ...>;
};

template<typename T, typename TypeList>
struct UniqueTypes
{
    static constexpr bool value = not TypeList::template contains<T>;
};

template<template <typename, typename> typename Filter, template <typename, typename> typename Filter2>
struct CombineFilters {
    template<typename T, typename TypeList>
    struct GenerateFilter
    {
        static constexpr bool value = Filter<T, TypeList>::value && Filter2<T, TypeList>::value;
    };
};

namespace Detail
{
    template <
        template <typename, typename> typename Filter,
        typename TypeList,
        typename Head,
        typename... Tail>
    struct TypeListGenerator
    {
        static constexpr bool UseCurrentType = Filter<Head, TypeList>::value;

        template<typename ListToAdd>
        using add_if_true = std::conditional_t<UseCurrentType,
                                        typename ListToAdd::template add<Head>,
                                        ListToAdd>;

        using current_type_list = add_if_true<TypeList>;

        using next_type = TypeListGenerator<Filter, current_type_list, Tail...>::type;

        using type = add_if_true<next_type>;
    };

    template <
        template <typename, typename> typename Filter,
        typename CurrentTypeList,
        typename Head>
    struct TypeListGenerator<Filter, CurrentTypeList, Head>
    {
        static constexpr bool UseCurrentType = Filter<Head, CurrentTypeList>::value;

        using next_type = TypeList<>;

        using type = std::conditional_t<UseCurrentType,
                                        typename next_type::template add<Head>,
                                        next_type>;
    };
}

template <
        template <typename, typename> typename Filter,
        typename... List>
using TypeListGenerator = Detail::TypeListGenerator<Filter, TypeList<>, List...>;