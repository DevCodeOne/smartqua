#pragma once

#include <limits>

template<typename T, typename = void>
struct DetermineSizeType {
    using Type = T;
};

template<typename T>
struct DetermineSizeType<T, std::void_t<std::underlying_type_t<T> > > {
    using Type = std::underlying_type_t<T>;
};

template<typename T>
using DetermineSizeTypeT = typename DetermineSizeType<T>::Type;

template<typename T>
concept Number = std::is_integral_v<T> || std::is_floating_point_v<T>;

// Extracted constexpr function for double cast comparison
template <Number Target, Number Source>
constexpr bool canBeExpressedInType(const Source& value) {
    if (std::is_unsigned_v<Target> != std::is_unsigned_v<Source> && value < 0) {
        return false;
    }
    return static_cast<Source>(static_cast<Target>(value)) == value;
}

// Main IsContainedInType logic
template <Number T, Number T2>
static constexpr bool IsContainedInType = []() {
    // Check if both types are either integral or floating-point
    if constexpr (std::is_integral_v<T> == std::is_integral_v<T2>) {
        return canBeExpressedInType<T>(std::numeric_limits<T2>::max()) &&
               canBeExpressedInType<T>(std::numeric_limits<T2>::min());
    }
    // Implicit fallback for mixed types
    return false;
}();

template<typename T, typename OT>
requires (IsContainedInType<T, OT>)
constexpr bool isInRange(T value, OT min, OT max) {
    return value >= min && value <= max;
}

template<typename OT>
constexpr bool isInRangeNoCheck(auto value, OT min, OT max) {
    if (!canBeExpressedInType<OT>(value) || !canBeExpressedInType<OT>(value)) {
        return false;
    }
    return value >= min && value <= max;
}

template<typename ValueType>
constexpr bool isWithinLimitsForType(const auto &value) {
    using SizeType = DetermineSizeTypeT<ValueType>;
    return isInRangeNoCheck(value, std::numeric_limits<SizeType>::min(), std::numeric_limits<SizeType>::max());
}

template<typename DestType, typename FromType>
    requires (std::is_convertible_v<FromType, DetermineSizeTypeT<DestType> >)
bool checkAssign(DestType &to_assign_to, FromType to_assign) {
    if (isWithinLimitsForType<DestType>(to_assign)) {
        to_assign_to = static_cast<DestType>(to_assign);
        return true;
    }

    return false;
}
