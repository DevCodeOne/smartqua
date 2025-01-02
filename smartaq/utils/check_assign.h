#pragma once

#include <limits>

template<typename T, typename = void>
struct FindSizeType {
    using Type = T;
};

template<typename T>
struct FindSizeType<T, std::void_t<std::underlying_type_t<T> > > {
    using Type = std::underlying_type_t<T>;
};

template<typename T>
using FindSizeTypeT = typename FindSizeType<T>::Type;

// Extracted constexpr function for double cast comparison
template <typename Source, typename Target>
constexpr bool doubleCastCompare(const Source& value) {
    return static_cast<Source>(static_cast<Target>(value)) == value;
}

template<typename T>
concept Number = std::is_integral_v<T> || std::is_floating_point_v<T>;

// Main IsContainedInType logic
template <Number T, Number T2>
static constexpr bool IsContainedInType = []() {
    // Check if both types are either integral or floating-point
    if constexpr (std::is_integral_v<T> == std::is_integral_v<T2>) {
        return doubleCastCompare<T2, T>(std::numeric_limits<T2>::max()) &&
               doubleCastCompare<T2, T>(std::numeric_limits<T2>::min());
    }
    // Implicit fallback for mixed types
    return false;
}();

template<typename T, typename OT>
requires (IsContainedInType<T, OT>)
constexpr bool isInRange(T value, OT min, OT max) {
    return value >= min && value <= max;
}

template<typename ValueType>
constexpr bool isWithinLimitsForType(const auto &value) {
    using SizeType = FindSizeTypeT<ValueType>;
    return isInRange(value, std::numeric_limits<SizeType>::min(), std::numeric_limits<SizeType>::max());
}

template<typename DestType, typename FromType>
    requires (std::is_convertible_v<FromType, FindSizeTypeT<DestType> >)
bool checkAssign(DestType &to_assign_to, FromType to_assign) {
    if (isWithinLimitsForType<DestType>(to_assign)) {
        to_assign_to = static_cast<DestType>(to_assign);
        return true;
    }

    return false;
}
