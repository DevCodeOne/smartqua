#pragma once

#include <array>

#include "utils/stack_string.h"

template<typename T>
concept ValidBaseType = requires(T a) {
    { T::StorageName } -> std::convertible_to<const char *>;
} && std::is_trivial_v<T>;

template<ValidBaseType BaseType>
struct SerializedRepresentation
{
    static const constexpr char *const StorageName{ BaseType::StorageName };

    BaseType value;
    bool initialized;
    BasicStackString<name_length> name;
};

// TODO: separate name_length
template<ValidBaseType BaseType, size_t Size>
    struct SerializedRepresentationCollection {
    using ValueType = SerializedRepresentation<BaseType>;

    static const constexpr char *const StorageName{ BaseType::StorageName };

    std::array<SerializedRepresentation<BaseType>, Size> values;
};
