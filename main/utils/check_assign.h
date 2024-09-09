#pragma once

#include <cstdint>
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

template<typename ValueType>
constexpr bool isWithinLimitsForType(const auto &value) {
    using SizeType = FindSizeTypeT<ValueType>;
    return std::numeric_limits<SizeType>::max() >= value && std::numeric_limits<SizeType>::min() <= value;
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
