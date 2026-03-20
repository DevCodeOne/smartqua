#pragma once

#include <type_traits>
#include <algorithm>

template<auto Value, typename ... Types>
requires (std::is_same_v<std::common_type_t<Types ...>, decltype(Value)>)
bool allEqualTo(Types ... arguments)
{
    return ((Value == arguments) && ... );
}

template<typename Range>
static constexpr bool IsUnique(Range &&range)
{
    std::array copy{range};
    std::ranges::sort(copy);
    return std::adjacent_find(copy.begin(), copy.end()) == copy.end();
}

