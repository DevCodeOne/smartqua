#pragma once

#include <array>

#include "utils/stack_string.h"
#include "utils/serialization/serializer.h"

template<typename T>
concept ValidBaseType = requires(T a) {
    { T::StorageName } -> std::convertible_to<const char *>;
} && std::is_trivial_v<T>;

template<ValidBaseType BaseType>
struct BinarySerializedRepresentation
{
    static const constexpr char *const StorageName{ BaseType::StorageName };

    bool operator==(const BinarySerializedRepresentation &other) const
    {
        if (not initialized || not other.initialized)
        {
            return false;
        }

        if (name != other.name)
        {
            return false;
        }

        return std::memcmp(&value, &other.value, sizeof(value)) == 0;
    }

    bool operator!=(const BinarySerializedRepresentation &other) const
    {
        return !(*this == other);
    }

    BaseType value;
    bool initialized;
    BasicStackString<name_length> name;
};

// TODO: separate name_length
template<ValidBaseType BaseType, size_t Size>
    struct BinarySerializedRepresentationCollection {
    using ValueType = BinarySerializedRepresentation<BaseType>;

    static const constexpr char *const StorageName{ BaseType::StorageName };

    auto begin() { return values.begin(); }
    auto end() { return values.end(); }

    auto begin() const { return values.begin(); }
    auto end() const { return values.end(); }

    std::array<BinarySerializedRepresentation<BaseType>, Size> values;
};


template<ValidBaseType T>
struct Serializer<BinarySerializedRepresentation<T>>
{
    template<typename Callback>
    static void serialize(const BinarySerializedRepresentation<T> &written, const BinarySerializedRepresentation<T> &value, Callback callback)
    {
        if (written == value)
        {
            return;
        }
        callback(value.name, value);
    }

    template<typename Callback>
    static void deserialize(BinarySerializedRepresentation<T> &value, Callback callback)
    {
        callback(value.name, value);
    }
};

// TODO: replace const char * -> name.data() with better type
template<ValidBaseType T, size_t Size>
struct Serializer<BinarySerializedRepresentationCollection<T, Size>>
{
    template<typename Callback>
    static void serialize(const BinarySerializedRepresentationCollection<T, Size> &writtenValue, const BinarySerializedRepresentationCollection<T, Size> &value, Callback callback)
    {
        for (const auto &[written, current] : std::views::zip(writtenValue, value))
        {
            if (written == current)
            {
                continue;
            }
            callback(current.name.data(), current.value);
        }
    }

    template<typename Callback>
    static void deserialize(BinarySerializedRepresentationCollection<T, Size> &value, Callback callback)
    {
        for (auto &current : value)
        {
            callback(current.name.data(), current.value);
        }
    }
};

