#pragma once

#include <type_traits>

template<typename T>
constexpr bool IsArithmeticInteger = std::is_integral_v<T> && !std::is_same_v<T, bool>;

template<typename T>
concept IsByteCopyable = std::is_standard_layout_v<T> && std::is_trivial_v<T>;

template<typename T>
concept IsEnumType = std::is_enum_v<T>;
