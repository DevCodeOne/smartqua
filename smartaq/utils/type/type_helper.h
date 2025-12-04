#pragma once

#include <type_traits>

template<typename T>
constexpr bool isArithmeticInteger = std::is_integral_v<T> && !std::is_same_v<T, bool>;