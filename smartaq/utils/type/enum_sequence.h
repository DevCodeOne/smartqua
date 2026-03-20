#pragma once

#include "utils/utils.h"
#include "utils/type/type_helper.h"

template<IsEnumType EnumType, EnumType ... EnumValues>
requires (IsUnique(std::array{EnumValues ...}))
struct EnumSequence
{
    static constexpr std::array values{EnumValues ...};
};
